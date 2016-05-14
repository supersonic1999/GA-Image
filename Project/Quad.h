#pragma once

#include "stdafx.h"
#include "GeneticAlgorithm.h"
#include <math.h>
#include <stdlib.h>
#include <glew\glew.h>
#include <glew\wglew.h>
#include <freeglut\freeglut.h>
#include <CoreStructures\CoreStructures.h>
#include <CGImport\CGModel\CGModel.h>
#include <CGImport\Importers\CGImporters.h>
#include "ShaderLoader.h"
#include "TextureLoader.h"

using namespace CoreStructures;
using namespace std;

class Quad
{
private:
	GLuint			terrainVertexBuffer;
	GLuint			terrainIndexBuffer;
	GLuint			terrainColourBuffer;
	GLuint			terrainVAO;
	GLuint			terrainShader;
	int				*terrainVertexIndices = NULL;
	int				seed;

public:	
	int				gridSize = 250;
	float			*terrainVertices = NULL;
	float			*terrainColours = NULL;	
	GeneticAlgorithm *GA;

	Quad()
	{
		seed = rand() % RAND_MAX;
		glGenVertexArrays(1, &terrainVAO);
		glGenBuffers(1, &terrainIndexBuffer);
		glGenBuffers(1, &terrainVertexBuffer);
		glGenBuffers(1, &terrainColourBuffer);
		
		terrainVertices = new float[gridSize*gridSize * 4]();
		terrainColours = new float[gridSize*gridSize * 4]();
		terrainVertexIndices = new int[gridSize*gridSize * 6]();

		int x = 0;
		int z = 0;
		int j = 0;

		for (int i = 0; i < gridSize*gridSize * 4; i += 4, x++)
		{
			if (i != 0 && i % (gridSize * 4) == 0)
				z++;

			//Colours
			terrainColours[i] = 1;
			terrainColours[i + 1] = 0;
			terrainColours[i + 2] = 0;
			terrainColours[i + 3] = 1;

			//Verts
			terrainVertices[i] = float((i / 4) % gridSize);
			terrainVertices[i + 1] = 0;
			terrainVertices[i + 2] = float(z);
			terrainVertices[i + 3] = 1;

			// Set Faces
			if (z + 1 < gridSize && ((x + 1) % gridSize != 0)) {
				//First Half
				terrainVertexIndices[j + 0] = gridSize + x;
				terrainVertexIndices[j + 1] = 1 + x;
				terrainVertexIndices[j + 2] = x;

				//Second Half
				terrainVertexIndices[j + 3] = (gridSize + 1) + x;
				terrainVertexIndices[j + 4] = 1 + x;
				terrainVertexIndices[j + 5] = gridSize + x;
				j += 6;
			}
		}

		// Setup VAO for terrain object
		glBindVertexArray(terrainVAO);

		// Setup VBO for vertex position data
		glBindBuffer(GL_ARRAY_BUFFER, terrainVertexBuffer);
		glBufferData(GL_ARRAY_BUFFER, gridSize * gridSize * 4 * sizeof(float), terrainVertices, GL_DYNAMIC_DRAW);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, (const GLvoid*)0); // attribute 0 gets data from bound VBO (so assign vertex position buffer to attribute 0)

		glBindBuffer(GL_ARRAY_BUFFER, terrainColourBuffer);
		glBufferData(GL_ARRAY_BUFFER, gridSize * gridSize * 4 * sizeof(float), terrainColours, GL_DYNAMIC_DRAW);
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, (const GLvoid*)0);

		glEnableVertexAttribArray(0); // Enable vertex position array
		glEnableVertexAttribArray(1);

		// Setup VBO for face index array	
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, terrainIndexBuffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, gridSize * gridSize * 6 * sizeof(int), terrainVertexIndices, GL_DYNAMIC_DRAW);

		// Unbind terrain VAO (or bind another VAO for another object / effect)
		// If we didn't do this, we may alter the bindings created above.
		glBindVertexArray(0);

		//
		// Load shaders
		//
		GLenum err = ShaderLoader::createShaderProgram(
			string("Resources\\Shaders\\quad_vertex.vs"),
			string("Resources\\Shaders\\quad_fragment.fs"),
			&terrainShader);
	}

	void display(GUMatrix4 viewProjectionMatrix)
	{
		if (GA->topImage != nullptr) {
			// Convert from char to float array
			for (int i = 0; i < gridSize; i++) {
				for (int j = 0; j < gridSize; j++) {
					terrainColours[4 * (i + (j * gridSize)) + 0] = GA->topImage[3 * (i + (j * gridSize)) + 0] / 255.0f;
					terrainColours[4 * (i + (j * gridSize)) + 1] = GA->topImage[3 * (i + (j * gridSize)) + 1] / 255.0f;
					terrainColours[4 * (i + (j * gridSize)) + 2] = GA->topImage[3 * (i + (j * gridSize)) + 2] / 255.0f;
					terrainColours[4 * (i + (j * gridSize)) + 3] = 1;
				}
			}

			glBindBuffer(GL_ARRAY_BUFFER, terrainColourBuffer);
			glBufferData(GL_ARRAY_BUFFER, gridSize * gridSize * 4 * sizeof(float), terrainColours, GL_DYNAMIC_DRAW);
			glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, (const GLvoid*)0);
		}
		// Store uniform locations from terrain and cube shader program objects
		static GLint terrainShader_viewProjMatrixLocation = glGetUniformLocation(terrainShader, "viewProjMatrix");
		static GLint terrainShader_modelMatrixLocation = glGetUniformLocation(terrainShader, "modelMatrix");

		//Use Shader
		glUseProgram(terrainShader);

		// Setup uniforms in terrainShader
		glUniformMatrix4fv(terrainShader_viewProjMatrixLocation, 1, GL_FALSE, (const GLfloat*)&viewProjectionMatrix);

		// Setup simple translation (model) matrix and pass to shader
		GUMatrix4 terrainModelTransform = GUMatrix4::translationMatrix((-gridSize / 2.0f) + 0.5f, 0.0f, (-gridSize / 2.0f) + 0.5f);
		glUniformMatrix4fv(terrainShader_modelMatrixLocation, 1, GL_FALSE, (const GLfloat*)&terrainModelTransform);

		// Draw terrain
		glBindVertexArray(terrainVAO);
		glDrawElements(GL_TRIANGLES, (gridSize*gridSize * 6), GL_UNSIGNED_INT, (const GLvoid*)0);

		glBindVertexArray(0);
	}
};
