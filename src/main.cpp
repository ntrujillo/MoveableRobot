#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <stack>
#include <cmath>
#include <iostream>
#include <chrono>
#include <thread>
#include "MatrixStack.h"
#include "Program.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 800

char* vertShaderPath = "../shaders/shader.vert";
char* fragShaderPath = "../shaders/shader.frag";

GLFWwindow *window;
double currentXpos, currentYpos;
glm::vec3 eye(0.0f, 0.0f, 10.0f);
glm::vec3 center(0.0f, 0.0f, 0.0f);
glm::vec3 up(0.0f, 1.0f, 0.0f);

Program program;
MatrixStack modelViewProjectionMatrix;

bool animationOn = false;

// Draw cube on screen
void DrawCube(glm::mat4& modelViewProjectionMatrix)
{
	program.SendUniformData(modelViewProjectionMatrix, "mvp");
	glDrawArrays(GL_TRIANGLES, 0, 36);
}

class RobotElement
{
    public:
        RobotElement()
        {
        }
        ~RobotElement()
        {
        }

		void setScale(glm::vec3 s) {
			scale = s;
			originalScale = s;
			selectedScale = {1.1f*s[0], 1.1f*s[1], 1.1f*s[2]};
		}

		void setParentTranslation(glm::vec3 t) {
			moveToParentTranslation = t;
		}

		void setJointTranslation(glm::vec3 t) {
			moveToJointTranslation = t;
		}

		void setRotation(glm::vec3 r) {
			rotation = r;
		}

		void addChild(RobotElement* child) {
			children.push_back(child);
		}

		std::vector<RobotElement*> getChildren() {
			return children;
		}

		void setParent(RobotElement* p) {
			parent = p;
		}

		RobotElement* getParent() {
			return parent;
		}

        // A member method for drawing itself and its children.
        // takes modelViewProjectionMatrix (pass by reference *aka smart pointer*)
        // update it by the trasnformation of each component
        void Draw(MatrixStack &stack)
        {
			// copy top
            modelViewProjectionMatrix.pushMatrix();

			/** multiply top by M2A **/
			modelViewProjectionMatrix.translate(moveToParentTranslation);
			/** *** **/

			/** multiply top by M1A **/
			modelViewProjectionMatrix.rotateX(rotation[0]);
            modelViewProjectionMatrix.rotateY(rotation[1]);
            modelViewProjectionMatrix.rotateZ(rotation[2]);
			/** *** **/

			/** multiply top by M*A **/
			modelViewProjectionMatrix.pushMatrix();
			modelViewProjectionMatrix.translate(moveToJointTranslation);
			modelViewProjectionMatrix.scale(scale);
			/** *** **/

			// draw cube
            DrawCube(modelViewProjectionMatrix.topMatrix());

			// pop M*A
            modelViewProjectionMatrix.popMatrix();

			for (int i = 0; i < children.size(); i ++) {
				children.at(i)->Draw(stack);
			}

			// pop extra
			modelViewProjectionMatrix.popMatrix();

        }

		// populate traversal vector
		void populateTraversalVector(std::vector<RobotElement*> &traversalVector) {
			// add element to the vector
			traversalVector.push_back(this);

			// add children to the vector
			for (int i = 0; i < children.size(); i++) {
				children.at(i)->populateTraversalVector(traversalVector);
			}
		}

		// select function for traversal
		void select()
		{
			scale = selectedScale;
		}

		// deselect function for traversal
		void deselect()
		{
			scale = originalScale;
		}

		void setName(std::string s) {
			elementName = s;
		}

		std::string getName() {
			return elementName;
		}

		void increaseRotation(char c) {
			float newAngle = 0.0;
			switch (c) {

				// z-axis
				case 'Z':
					// increment z angle
					newAngle = rotation[2] + glm::radians(5.0f);
					
					// update rotation
					setRotation({rotation[0], rotation[1], newAngle});
					break;

				// y-axis
				case 'Y':
					// increment y angle
					newAngle = rotation[1] + glm::radians(5.0f);

					// update rotation
					setRotation({rotation[0], newAngle, rotation[2]});
					break;

				case 'X':
					// increment x angle
					newAngle = rotation[0] + glm::radians(5.0f);

					// update rotation
					setRotation({newAngle, rotation[1], rotation[2]});
					break;
			}
		}

		void decreaseRotation(char c) {
			float newAngle = 0.0;
			switch (c) {

				// z-axis
				case 'z':
					// increment z angle
					newAngle = rotation[2] - glm::radians(5.0f);
					
					// update rotation
					setRotation({rotation[0], rotation[1], newAngle});
					break;

				// y-axis
				case 'y':
					// increment y angle
					newAngle = rotation[1] - glm::radians(5.0f);

					// update rotation
					setRotation({rotation[0], newAngle, rotation[2]});
					break;

				case 'x':
					// increment x angle
					newAngle = rotation[0] - glm::radians(5.0f);

					// update rotation
					setRotation({newAngle, rotation[1], rotation[2]});
					break;
			}
		}

    private:
        // child element(s)
        std::vector<RobotElement*> children;

		// parent element
		RobotElement* parent = nullptr;

        // translation of this component’s joint with respect to the parent component’s joint
        glm::vec3 moveToParentTranslation{0.0f, 0.0f, 0.0f};

        // the current joint angles about the X, Y, and Z axes of the component’s joint.
        // (You may want to start with Z-rotations only.) ??? why 
        glm::vec3 rotation{0.0f, 0.0f, 0.0f};

        // translation of the component with respect to its joint.
        glm::vec3 moveToJointTranslation{0.0f, 0.0f, 0.0f};

        // the X, Y, and Z scaling factors for the component.
        glm::vec3 scale{0.0f, 0.0f, 0.0f};

		glm::vec3 originalScale{0.0f, 0.0f, 0.0f};

		glm::vec3 selectedScale{0.0f, 0.0f, 0.0f};

		std::string elementName = "";

};
// create the root element globally
RobotElement* robotTorso = new RobotElement();
std::vector<RobotElement*> traversalVector;
int currentIndex = 0;

void ConstructRobot()
{
	// left lower arm
	RobotElement* robotLeftLowerArm = new RobotElement();
	glm::vec3 scale{0.25, 0.5, 0.25};
	glm::vec3 jointTranslation{0.0f, -0.4f, 0.0f};
	glm::vec3 rotation{0.0f, 0.0f, 0.0f};
	glm::vec3 parentTranslation{0.0f, -2.0f, 0.0f};
	robotLeftLowerArm->setScale(scale);
	robotLeftLowerArm->setJointTranslation(jointTranslation);
	robotLeftLowerArm->setRotation(rotation);
	robotLeftLowerArm->setParentTranslation(parentTranslation);
	robotLeftLowerArm->setName("Left Lower Arm");

	// left upper arm
	RobotElement* robotLeftUpperArm = new RobotElement();
	robotLeftLowerArm->setParent(robotLeftUpperArm);
	scale = {0.5, 1.0, 0.5};
	jointTranslation = {0.0f, -0.9f, 0.0f};
	rotation = {0.0f, 0.0f, glm::radians(90.0f)};
	parentTranslation = {1.0f, 1.0f, 0.0f};
	robotLeftUpperArm->setScale(scale);
	robotLeftUpperArm->setJointTranslation(jointTranslation);
	robotLeftUpperArm->setRotation(rotation);
	robotLeftUpperArm->setParentTranslation(parentTranslation);
	robotLeftUpperArm->addChild(robotLeftLowerArm);
	robotLeftUpperArm->setName("Left Upper Arm");
	robotLeftUpperArm->setParent(robotTorso);

	// left lower leg
	RobotElement* robotLeftLowerLeg = new RobotElement();
	scale = {0.25, 0.5, 0.25};
	jointTranslation = {0.0f, -0.4f, 0.0f};
	rotation = {0.0f, 0.0f, 0.0f};
	parentTranslation = {0.0f, -2.0f, 0.0f};
	robotLeftLowerLeg->setScale(scale);
	robotLeftLowerLeg->setJointTranslation(jointTranslation);
	robotLeftLowerLeg->setRotation(rotation);
	robotLeftLowerLeg->setParentTranslation(parentTranslation);
	robotLeftLowerLeg->setName("Left Lower Leg");

	// left upper leg
	RobotElement* robotLeftUpperLeg = new RobotElement();
	robotLeftLowerLeg->setParent(robotLeftUpperLeg);
	scale = {0.5, 1.0, 0.5};
	jointTranslation = {0.0f, -0.9f, 0.0f};
	rotation = {0.0f, 0.0f, 0.0f};
	parentTranslation = {0.6f, -2.0f, 0.0f};
	robotLeftUpperLeg->setScale(scale);
	robotLeftUpperLeg->setJointTranslation(jointTranslation);
	robotLeftUpperLeg->setRotation(rotation);
	robotLeftUpperLeg->setParentTranslation(parentTranslation);
	robotLeftUpperLeg->addChild(robotLeftLowerLeg);
	robotLeftUpperLeg->setName("Left Upper Leg");
	robotLeftUpperLeg->setParent(robotTorso);

	// right lower arm
	RobotElement* robotRightLowerArm = new RobotElement();
	scale = {0.25, 0.5, 0.25};
	jointTranslation = {0.0f, -0.4f, 0.0f};
	rotation = {0.0f, 0.0f, 0.0f};
	parentTranslation = {0.0f, -2.0f, 0.0f};
	robotRightLowerArm->setScale(scale);
	robotRightLowerArm->setJointTranslation(jointTranslation);
	robotRightLowerArm->setRotation(rotation);
	robotRightLowerArm->setParentTranslation(parentTranslation);
	robotRightLowerArm->setName("Right Lower Arm");

	// right upper arm
	RobotElement* robotRightUpperArm = new RobotElement();
	robotRightLowerArm->setParent(robotRightUpperArm);
	scale = {0.5, 1.0, 0.5};
	jointTranslation = {0.0f, -0.9f, 0.0f};
	rotation = {0.0f, 0.0f, glm::radians(-90.0f)};
	parentTranslation = {-1.0f, 1.0f, 0.0f};
	robotRightUpperArm->setScale(scale);
	robotRightUpperArm->setJointTranslation(jointTranslation);
	robotRightUpperArm->setRotation(rotation);
	robotRightUpperArm->setParentTranslation(parentTranslation);
	robotRightUpperArm->addChild(robotRightLowerArm);
	robotRightUpperArm->setName("Right Upper Arm");
	robotRightUpperArm->setParent(robotTorso);
	

	// right lower leg
	RobotElement* robotRightLowerLeg = new RobotElement();
	scale = {0.25, 0.5, 0.25};
	jointTranslation = {0.0f, -0.4f, 0.0f};
	rotation = {0.0f, 0.0f, 0.0f};
	parentTranslation = {0.0f, -2.0f, 0.0f};
	robotRightLowerLeg->setScale(scale);
	robotRightLowerLeg->setJointTranslation(jointTranslation);
	robotRightLowerLeg->setRotation(rotation);
	robotRightLowerLeg->setParentTranslation(parentTranslation);
	robotRightLowerLeg->setName("Right Lower Leg");

	// right upper leg
	RobotElement* robotRightUpperLeg = new RobotElement();
	robotRightLowerLeg->setParent(robotRightUpperLeg);
	scale = {0.5, 1.0, 0.5};
	jointTranslation = {0.0f, -0.9f, 0.0f};
	rotation = {0.0f, 0.0f, 0.0f};
	parentTranslation = {-0.6f, -2.0f, 0.0f};
	robotRightUpperLeg->setScale(scale);
	robotRightUpperLeg->setJointTranslation(jointTranslation);
	robotRightUpperLeg->setRotation(rotation);
	robotRightUpperLeg->setParentTranslation(parentTranslation);
	robotRightUpperLeg->addChild(robotRightLowerLeg);
	robotRightUpperLeg->setName("Right Upper Leg");
	robotRightUpperLeg->setParent(robotTorso);

	// head
	RobotElement* robotHead = new RobotElement();
	scale = {0.5, 0.5, 0.5};
	jointTranslation = {0.0f, 0.4f, 0.0f};
	rotation = {0.0f, 0.0f, 0.0f};
	parentTranslation = {0.0f, 2.0f, 0.0f};
	robotHead->setScale(scale);
	robotHead->setJointTranslation(jointTranslation);
	robotHead->setRotation(rotation);
	robotHead->setParentTranslation(parentTranslation);
	robotHead->setName("Head");
	robotHead->setParent(robotTorso);

	// torso
	scale = {1.0f, 2.0f, 1.0f};
	robotTorso->setScale(scale);
	robotTorso->addChild(robotLeftUpperArm);
	robotTorso->addChild(robotRightUpperArm);
	robotTorso->addChild(robotLeftUpperLeg);
	robotTorso->addChild(robotRightUpperLeg);
	robotTorso->addChild(robotHead);
	robotTorso->setName("Torso");

	// construct the traversal vector
	robotTorso->populateTraversalVector(traversalVector);
	
	// select torso
	robotTorso->select();
}

void startAnimation() {
	std::chrono::milliseconds timespan(500); // or whatever
	for(int i = 0; i < 20; i ++) {
		// start timer

		// for (int i = 0; i < robotTorso->getChildren().size(); i ++ ) {
			// leg angle = 90sin(0.5 * glfwGetTime())

			// increment z angle
			// float newAngle = 90*sin(0.5 * glfwGetTime());
			// std::cout << "angle: " << newAngle << std::endl;
			
			// update rotation
			// robotTorso->setRotation({0.0f, 0.0f, glm::radians(newAngle)});
			// robotTorso->Draw(modelViewProjectionMatrix);

			robotTorso->increaseRotation('Z');

			std::this_thread::sleep_for(timespan);
		// }
	}

}

void Display()
{	
	program.Bind();

	modelViewProjectionMatrix.loadIdentity();
	modelViewProjectionMatrix.pushMatrix();

	// Setting the view and Projection matrices
	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	modelViewProjectionMatrix.Perspective(glm::radians(60.0f), float(width) / float(height), 0.1f, 100.0f);
	modelViewProjectionMatrix.LookAt(eye, center, up);
	
	robotTorso->Draw(modelViewProjectionMatrix);
	modelViewProjectionMatrix.popMatrix();

	program.Unbind();
	
}

// Scroll callback function
void ScrollCallback(GLFWwindow* lwindow, double xoffset, double yoffset)
{
	// ignore xoffset since we are only responding to normal scrolling

	// calculate the vector centerToEye
	glm::vec3 centerToEye = eye - center;

	// if scroll down
	if (yoffset < 0) {
		// zoom out
		centerToEye = centerToEye * 1.05f; // scale centerToEye
		eye = center + centerToEye; // update camera position
	} else {
		// zoom in
		centerToEye = centerToEye / 1.05f; // scale centerToEye
		eye = center + centerToEye; // update camera position
	}
}


// Mouse callback function
void MouseCallback(GLFWwindow* lWindow, int button, int action, int mods)
{

}

// store previous mouse positions
glm::vec2 prevCursorPosition = {0.0f, 0.0f};

// Mouse position callback function
void CursorPositionCallback(GLFWwindow* lWindow, double xpos, double ypos)
{
	// get new cursor position
	glm::vec2 currentCursorPosition = {(float) xpos, (float) ypos};

	// get cursor position delta
	glm::vec2 cursorPositionDelta = prevCursorPosition - currentCursorPosition;
	
	int l_state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
	if (l_state == GLFW_PRESS) {

		// calculate the vector centerToEye
		glm::vec3 centerToEye = eye - center;

		// calculate horizontal rotation
		glm::mat4 horizontal_identity_matrix(1.0f);
		glm::mat3 horizontal_rotation = glm::mat3(glm::rotate(horizontal_identity_matrix, cursorPositionDelta.x * (0.05f), up));

		// calculate vertical rotation
		glm::mat4 vertical_identity_matrix(1.0f);
		glm::vec3 right = glm::cross(eye, up); // calculate right matrix for the vertical rotation
		glm::mat3 vertical_rotation = glm::mat3(glm::rotate(vertical_identity_matrix, cursorPositionDelta.y * (0.05f), right));

		// update the new centerToEye vector with new rotations (wrt origin)
		centerToEye = vertical_rotation * horizontal_rotation * centerToEye;

		// now add center to centerToEye to get the final placement of camera
		eye = center + centerToEye;
		up = vertical_rotation * up;
	}

	int r_state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);
	if (r_state == GLFW_PRESS) {
		eye = {eye[0] - cursorPositionDelta.x * (0.05f), eye[1] + cursorPositionDelta.y * (0.05f), eye[2]};
		center = {center[0] - cursorPositionDelta.x * (0.05f), center[1] + cursorPositionDelta.y * (0.05f), center[2]};
	}

	// update prev cursor position
	prevCursorPosition = currentCursorPosition;
}


// Keyboard character callback function
void CharacterCallback(GLFWwindow* lWindow, unsigned int key)
{
	// std::cout << "Key " << (char)key << " is pressed." << std::endl;

	switch (key) {

		// traverse hierarchy forward
		case '.':
			// deselect current
			traversalVector.at(currentIndex)->deselect();

			currentIndex++;

			// reset to 0 if over index
			if (currentIndex == traversalVector.size()) {
				currentIndex = 0;
			}

			// select the current index
			traversalVector.at(currentIndex)->select();

			break;

		// traverse hierarchy backward
		case ',':
			// deselect current
			traversalVector.at(currentIndex)->deselect();

			currentIndex--;

			// reset to 0 if over index
			if (currentIndex == -1) {
				currentIndex = traversalVector.size() - 1;
			}

			// select the current index
			traversalVector.at(currentIndex)->select();

			break;
		
		// increase Z angle
		case 'Z':
			traversalVector.at(currentIndex)->increaseRotation('Z');
			break;

		// decrease Z angle
		case 'z':
			traversalVector.at(currentIndex)->decreaseRotation('z');
			break;
		
		// increase Y angle
		case 'Y':
			traversalVector.at(currentIndex)->increaseRotation('Y');
			break;
		
		// decrease Y angle
		case 'y':
			traversalVector.at(currentIndex)->decreaseRotation('y');
			break;

		// increase X angle
		case 'X':
			traversalVector.at(currentIndex)->increaseRotation('X');
			break;
		
		// decrease X angle
		case 'x':
			traversalVector.at(currentIndex)->decreaseRotation('x');
			break;

		// toggle animation
		case 'r':
			animationOn = !animationOn;
			startAnimation();
			break;
	}
}

void CreateCube()
{
	// x, y, z, r, g, b, ...
	float cubeVerts[] = {
		// Face x-
		-1.0f,	+1.0f,	+1.0f,	0.8f,	0.2f,	0.2f,
		-1.0f,	+1.0f,	-1.0f,	0.8f,	0.2f,	0.2f,
		-1.0f,	-1.0f,	+1.0f,	0.8f,	0.2f,	0.2f,
		-1.0f,	-1.0f,	+1.0f,	0.8f,	0.2f,	0.2f,
		-1.0f,	+1.0f,	-1.0f,	0.8f,	0.2f,	0.2f,
		-1.0f,	-1.0f,	-1.0f,	0.8f,	0.2f,	0.2f,
		// Face x+
		+1.0f,	+1.0f,	+1.0f,	0.8f,	0.2f,	0.2f,
		+1.0f,	-1.0f,	+1.0f,	0.8f,	0.2f,	0.2f,
		+1.0f,	+1.0f,	-1.0f,	0.8f,	0.2f,	0.2f,
		+1.0f,	+1.0f,	-1.0f,	0.8f,	0.2f,	0.2f,
		+1.0f,	-1.0f,	+1.0f,	0.8f,	0.2f,	0.2f,
		+1.0f,	-1.0f,	-1.0f,	0.8f,	0.2f,	0.2f,
		// Face y-
		+1.0f,	-1.0f,	+1.0f,	0.2f,	0.8f,	0.2f,
		-1.0f,	-1.0f,	+1.0f,	0.2f,	0.8f,	0.2f,
		+1.0f,	-1.0f,	-1.0f,	0.2f,	0.8f,	0.2f,
		+1.0f,	-1.0f,	-1.0f,	0.2f,	0.8f,	0.2f,
		-1.0f,	-1.0f,	+1.0f,	0.2f,	0.8f,	0.2f,
		-1.0f,	-1.0f,	-1.0f,	0.2f,	0.8f,	0.2f,
		// Face y+
		+1.0f,	+1.0f,	+1.0f,	0.2f,	0.8f,	0.2f,
		+1.0f,	+1.0f,	-1.0f,	0.2f,	0.8f,	0.2f,
		-1.0f,	+1.0f,	+1.0f,	0.2f,	0.8f,	0.2f,
		-1.0f,	+1.0f,	+1.0f,	0.2f,	0.8f,	0.2f,
		+1.0f,	+1.0f,	-1.0f,	0.2f,	0.8f,	0.2f,
		-1.0f,	+1.0f,	-1.0f,	0.2f,	0.8f,	0.2f,
		// Face z-
		+1.0f,	+1.0f,	-1.0f,	0.2f,	0.2f,	0.8f,
		+1.0f,	-1.0f,	-1.0f,	0.2f,	0.2f,	0.8f,
		-1.0f,	+1.0f,	-1.0f,	0.2f,	0.2f,	0.8f,
		-1.0f,	+1.0f,	-1.0f,	0.2f,	0.2f,	0.8f,
		+1.0f,	-1.0f,	-1.0f,	0.2f,	0.2f,	0.8f,
		-1.0f,	-1.0f,	-1.0f,	0.2f,	0.2f,	0.8f,
		// Face z+
		+1.0f,	+1.0f,	+1.0f,	0.2f,	0.2f,	0.8f,
		-1.0f,	+1.0f,	+1.0f,	0.2f,	0.2f,	0.8f,
		+1.0f,	-1.0f,	+1.0f,	0.2f,	0.2f,	0.8f,
		+1.0f,	-1.0f,	+1.0f,	0.2f,	0.2f,	0.8f,
		-1.0f,	+1.0f,	+1.0f,	0.2f,	0.2f,	0.8f,
		-1.0f,	-1.0f,	+1.0f,	0.2f,	0.2f,	0.8f
	};

	GLuint vertBufferID;
	glGenBuffers(1, &vertBufferID);
	glBindBuffer(GL_ARRAY_BUFFER, vertBufferID);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVerts), cubeVerts, GL_STATIC_DRAW);
	GLint posID = glGetAttribLocation(program.GetPID(), "position");
	glEnableVertexAttribArray(posID);
	glVertexAttribPointer(posID, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), 0);
	GLint colID = glGetAttribLocation(program.GetPID(), "color");
	glEnableVertexAttribArray(colID);
	glVertexAttribPointer(colID, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));

}

void FrameBufferSizeCallback(GLFWwindow* lWindow, int width, int height)
{
	glViewport(0, 0, width, height);
}

void Init()
{
	glfwInit();
	glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GL_FALSE);
	window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Moveable Robot - Nathaniel Trujillo", NULL, NULL);
	glfwMakeContextCurrent(window);
	glewExperimental = GL_TRUE;
	glewInit();
	glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
	glfwSetScrollCallback(window, ScrollCallback);
	glfwSetMouseButtonCallback(window, MouseCallback);
	glfwSetCursorPosCallback(window, CursorPositionCallback);
	glfwSetCharCallback(window, CharacterCallback);
	glfwSetFramebufferSizeCallback(window, FrameBufferSizeCallback);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glEnable(GL_DEPTH_TEST);

	program.SetShadersFileName(vertShaderPath, fragShaderPath);
	program.Init();

	CreateCube();
	ConstructRobot();
}


int main()
{	
	Init();
	while ( glfwWindowShouldClose(window) == 0) 
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		Display();
		glFlush();
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glfwTerminate();
	return 0;
}