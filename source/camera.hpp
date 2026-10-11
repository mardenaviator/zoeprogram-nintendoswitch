#ifndef CAMERA_H
#define CAMERA_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


// Defines several possible options for camera movement. Used as abstraction to stay away from window-system specific input methods
enum Camera_Movement {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    NONE
};


    

// Default camera values
const float YAW         = -90.0f;
const float PITCH       =  0.0f;
const float SPEED       =  5.5f;
const float SENSITIVITY =  0.9f;
const float ZOOM        =  45.0f;


// An abstract camera class that processes input and calculates the corresponding Euler Angles, Vectors and Matrices for use in OpenGL
class Camera
{
public:
    // camera Attributes
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;
    // euler Angles
    float Yaw;
    float Pitch;
    // camera options
    float MovementSpeed;
    float MouseSensitivity;
    float Zoom;

    // constructor with vectors
    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = YAW, float pitch = PITCH) : Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY), Zoom(ZOOM)
    {
        Position = position;
        WorldUp = up;
        Yaw = yaw;
        Pitch = pitch;
        updateCameraVectors();
    }
    // constructor with scalar values
    Camera(float posX, float posY, float posZ, float upX, float upY, float upZ, float yaw, float pitch) : Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY), Zoom(ZOOM)
    {
        Position = glm::vec3(posX, posY, posZ);
        WorldUp = glm::vec3(upX, upY, upZ);
        Yaw = yaw;
        Pitch = pitch;
        updateCameraVectors();
    }

    // returns the view matrix calculated using Euler Angles and the LookAt Matrix
    glm::mat4 GetViewMatrix()
    {
        return glm::lookAt(Position, Position + Front, Up);
    }



    
    // processes input received from any keyboard-like input system. Accepts input parameter in the form of camera defined ENUM (to abstract it from windowing systems)
    void ProcessKeyboard(u32 kHeld, PadState *pad, float deltaTime)
    {
        float velocity = MovementSpeed * deltaTime;

            HidAnalogStickState analog_stick_l = padGetStickPos(pad, 0);
            float stickX = analog_stick_l.x / 32768.0f;
            float stickY = analog_stick_l.y / 32768.0f;
            if (stickY > 0)   Position += Front * velocity * stickY;
            if (stickY < 0)   Position += Front * velocity * stickY;
            if (stickX > 0)   Position += Right * velocity * stickX;
            if (stickX < 0)   Position += Right * velocity * stickX;


        Camera_Movement mov = Camera_Movement::NONE;  // сбрасываем каждый кадр
        if (kHeld & HidNpadButton_Up)    mov = Camera_Movement::FORWARD;
        if (kHeld & HidNpadButton_Down)  mov = Camera_Movement::BACKWARD;
        if (kHeld & HidNpadButton_Left)  mov = Camera_Movement::LEFT;
        if (kHeld & HidNpadButton_Right) mov = Camera_Movement::RIGHT;
        
        if (mov == FORWARD)
            Position += Front * velocity;
        if (mov == BACKWARD)
            Position -= Front * velocity;
        if (mov == LEFT)
            Position -= Right * velocity;
        if (mov == RIGHT)
            Position += Right * velocity;
    }

    // processes input received from a mouse input system. Expects the offset value in both the x and y direction.
    void ProcessMouseMovement(PadState * pad, float deltaTime, GLboolean constrainPitch = true)
    {
    float xoffset = 0;
    float yoffset = 0;
       
        HidAnalogStickState analog_stick_r = padGetStickPos(pad, 1);
        float stickX = analog_stick_r.x / 32768.0f;
        float stickY = analog_stick_r.y / 32768.0f;
        float deadZone = 0.15f;
        float length = sqrtf(stickX*stickX + stickY*stickY);
        if (length > deadZone) {
    
    float sensitivity = 2.8f;  
    xoffset = stickX * sensitivity * deltaTime * 60.0f;  
    yoffset = stickY * sensitivity * deltaTime * 60.0f;
    
            }



        xoffset *= MouseSensitivity;
        yoffset *= MouseSensitivity;

        Yaw   += xoffset;
        Pitch += yoffset;

        // make sure that when pitch is out of bounds, screen doesn't get flipped
        if (constrainPitch)
        {
            if (Pitch > 89.0f)
                Pitch = 89.0f;
            if (Pitch < -89.0f)
                Pitch = -89.0f;
        }

        // update Front, Right and Up Vectors using the updated Euler angles
        updateCameraVectors();
    }

    void stopgoing(Model * model) {
        glm::vec3 vector;
        for (int i = 0; i < model->meshes.size(); i++) {
                for (int j =0; j < model->meshes[i].vertices.size(); j ++) {
                    
                    
                    vector.x = model->meshes[i].vertices[j].Position.x ;
                    vector.y = model->meshes[i].vertices[j].Position.y ;
                    vector.z = model->meshes[i].vertices[j].Position.z ;
                    if (this->Position == vector) this->Position = glm::vec3(0.0f, 0.0f, -3.0f);
                }

        };
    };

    // processes input received from a mouse scroll-wheel event. Only requires input on the vertical wheel-axis
    void ProcessMouseScroll(float yoffset)
    {
        Zoom -= (float)yoffset;
        if (Zoom < 1.0f)
            Zoom = 1.0f;
        if (Zoom > 45.0f)
            Zoom = 45.0f;
    }

private:
    // calculates the front vector from the Camera's (updated) Euler Angles
    void updateCameraVectors()
    {
        // calculate the new Front vector
        glm::vec3 front;
        front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        front.y = sin(glm::radians(Pitch));
        front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        Front = glm::normalize(front);
        // also re-calculate the Right and Up vector
        Right = glm::normalize(glm::cross(Front, WorldUp));  // normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
        Up    = glm::normalize(glm::cross(Right, Front));
    }
};
#endif