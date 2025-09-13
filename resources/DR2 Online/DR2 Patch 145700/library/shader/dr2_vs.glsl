#version 400

layout (location = 0) in vec4 v4pos;
layout (location = 1) in vec4 v4diffuse;
layout (location = 2) in vec4 v4specular;
layout (location = 3) in vec2 v2uv;

uniform mat4 mat_Model;
uniform mat4 mat_View;
uniform mat4 mat_Proj;
uniform mat4 mat_Ident;
uniform int screenWidth = 1920;
uniform int screenHeight = 1080;

out vec4 diffuse;
out vec4 specular;
out vec2 TexCoord;
out vec4 pos;

void main() {
	gl_Position = mat_Ident * vec4((v4pos.x-(screenWidth/2.0f)) / (screenWidth/2.0f), (v4pos.y-(screenHeight/2.0f)) / (screenHeight/2.0f), v4pos.z, 1.0f);
    diffuse = v4diffuse;
    specular = v4specular;
    TexCoord = v2uv;

    pos = gl_Position;
};
