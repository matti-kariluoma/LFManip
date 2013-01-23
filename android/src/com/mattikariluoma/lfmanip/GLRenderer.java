/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
 /**
  * Cribbed from the OpenGLES tutorial:
  * 	http://developer.android.com/training/graphics/opengl/touch.html
  * 	http://developer.android.com/shareables/training/OpenGLES.zip
  *
  * Changelog:
  *
  *
  * Matti Kariluoma Jan 2013 <matti@kariluo.ma>
  */

package com.mattikariluoma.lfmanip;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.nio.ShortBuffer;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.opengl.Matrix;
import android.util.Log;

public class GLRenderer implements GLSurfaceView.Renderer
{
	private static final String TAG = "GLRenderer";
	private Triangle mTriangle;
	private Square   mSquare;

	private final float[] mMVPMatrix = new float[16];
	private final float[] mProjMatrix = new float[16];
	private final float[] mVMatrix = new float[16];
	private final float[] mRotationMatrix = new float[16];

	// Declare volatile; we update it from other threads
	public volatile float mAngle;

	@Override
	public void onSurfaceCreated(GL10 unused, EGLConfig config)
	{
		// report supported extensions
		if (BuildConfig.DEBUG)
		{
			Log.d(TAG, unused.glGetString(GL10.GL_EXTENSIONS));
		}

		// Set the background frame color
		GLES20.glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		
		mTriangle = new Triangle();
		mSquare   = new Square();
	}

	@Override
	public void onDrawFrame(GL10 unused)
	{
		// Draw background color
		GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);

		// Set the camera position (View matrix)
		Matrix.setLookAtM(mVMatrix, 0, 0, 0, -3, 0f, 0f, 0f, 0f, 1.0f, 0.0f);

		// Calculate the projection and view transformation
		Matrix.multiplyMM(mMVPMatrix, 0, mProjMatrix, 0, mVMatrix, 0);

		// Draw square
		mSquare.draw(mMVPMatrix);

		// Create a rotation for the triangle
		//long time = SystemClock.uptimeMillis() % 4000L;
		//float angle = 0.090f * ((int) time);
		Matrix.setRotateM(mRotationMatrix, 0, mAngle, 0, 0, -1.0f);

		// Combine the rotation matrix with the projection and camera view
		Matrix.multiplyMM(mMVPMatrix, 0, mRotationMatrix, 0, mMVPMatrix, 0);

		// Draw triangle
		mTriangle.draw(mMVPMatrix);
	}

	@Override
	public void onSurfaceChanged(GL10 unused, int width, int height)
	{
		// Adjust the viewport based on geometry changes,
		// such as screen rotation
		GLES20.glViewport(0, 0, width, height);

		float ratio = (float) width / height;

		// this projection matrix is applied to object coordinates
		// in the onDrawFrame() method
		Matrix.frustumM(mProjMatrix, 0, -ratio, ratio, -1, 1, 3, 7);
	}
	
	@Override
	public static int loadShader(int type, String shaderCode)
	{
		// create a vertex shader type (GLES20.GL_VERTEX_SHADER)
		// or a fragment shader type (GLES20.GL_FRAGMENT_SHADER)
		int shader = GLES20.glCreateShader(type);

		// add the source code to the shader and compile it
		GLES20.glShaderSource(shader, shaderCode);
		GLES20.glCompileShader(shader);

		return shader;
	}

	/**
	* Utility method for debugging OpenGL calls. Provide the name of the call
	* just after making it:
	*
	* <pre>
	* mColorHandle = GLES20.glGetUniformLocation(mProgram, "vColor");
	* GLRenderer.checkGlError("glGetUniformLocation");
	* </pre>
	*
	* If the operation is not successful, the check throws an error.
	*
	* @param glOperation - Name of the OpenGL call to check.
	*/
	public static void checkGlError(String glOperation)
	{
		int error;
		while ((error = GLES20.glGetError()) != GLES20.GL_NO_ERROR)
		{
			Log.e(TAG, glOperation + ": glError " + error);
			throw new RuntimeException(glOperation + ": glError " + error);
		}
	}
}

class Triangle
{
	private final String vertexShaderCode =
			// This matrix member variable provides a hook to manipulate
			// the coordinates of the objects that use this vertex shader
			"uniform mat4 uMVPMatrix;" +

			"attribute vec4 vPosition;" +
			"void main() {" +
			// the matrix must be included as a modifier of gl_Position
			"  gl_Position = vPosition * uMVPMatrix;" +
			"}";

	private final String fragmentShaderCode =
			"precision mediump float;" +
			"uniform vec4 vColor;" +
			"void main() {" +
			"  gl_FragColor = vColor;" +
			"}";

	private final FloatBuffer vertexBuffer;
	private final int mProgram;
	private int mPositionHandle;
	private int mColorHandle;
	private int mMVPMatrixHandle;

	// number of coordinates per vertex in this array
	static final int COORDS_PER_VERTEX = 3;
	static float triangleCoords[] = { // in counterclockwise order:
			 0.0f,  0.622008459f, 0.0f,   // top
			-0.5f, -0.311004243f, 0.0f,   // bottom left
			 0.5f, -0.311004243f, 0.0f    // bottom right
	};
	private final int vertexCount = triangleCoords.length / COORDS_PER_VERTEX;
	private final int vertexStride = COORDS_PER_VERTEX * 4; // 4 bytes per vertex

	// Set color with red, green, blue and alpha (opacity) values
	float color[] = { 0.63671875f, 0.76953125f, 0.22265625f, 1.0f };

	public Triangle()
	{
		// initialize vertex byte buffer for shape coordinates
		ByteBuffer bb = ByteBuffer.allocateDirect(
				// (number of coordinate values * 4 bytes per float)
				triangleCoords.length * 4);
		// use the device hardware's native byte order
		bb.order(ByteOrder.nativeOrder());

		// create a floating point buffer from the ByteBuffer
		vertexBuffer = bb.asFloatBuffer();
		// add the coordinates to the FloatBuffer
		vertexBuffer.put(triangleCoords);
		// set the buffer to read the first coordinate
		vertexBuffer.position(0);

		// prepare shaders and OpenGL program
		int vertexShader = GLRenderer.loadShader(GLES20.GL_VERTEX_SHADER,
																							 vertexShaderCode);
		int fragmentShader = GLRenderer.loadShader(GLES20.GL_FRAGMENT_SHADER,
																								 fragmentShaderCode);

		mProgram = GLES20.glCreateProgram();             // create empty OpenGL Program
		GLES20.glAttachShader(mProgram, vertexShader);   // add the vertex shader to program
		GLES20.glAttachShader(mProgram, fragmentShader); // add the fragment shader to program
		GLES20.glLinkProgram(mProgram);                  // create OpenGL program executables
	}
	
	@Override
	public void draw(float[] mvpMatrix)
	{
		// Add program to OpenGL environment
		GLES20.glUseProgram(mProgram);

		// get handle to vertex shader's vPosition member
		mPositionHandle = GLES20.glGetAttribLocation(mProgram, "vPosition");

		// Enable a handle to the triangle vertices
		GLES20.glEnableVertexAttribArray(mPositionHandle);

		// Prepare the triangle coordinate data
		GLES20.glVertexAttribPointer(mPositionHandle, COORDS_PER_VERTEX,
				GLES20.GL_FLOAT, false, vertexStride, vertexBuffer);

		// get handle to fragment shader's vColor member
		mColorHandle = GLES20.glGetUniformLocation(mProgram, "vColor");

		// Set color for drawing the triangle
		GLES20.glUniform4fv(mColorHandle, 1, color, 0);

		// get handle to shape's transformation matrix
		mMVPMatrixHandle = GLES20.glGetUniformLocation(mProgram, "uMVPMatrix");
		GLRenderer.checkGlError("glGetUniformLocation");

		// Apply the projection and view transformation
		GLES20.glUniformMatrix4fv(mMVPMatrixHandle, 1, false, mvpMatrix, 0);
		GLRenderer.checkGlError("glUniformMatrix4fv");

		// Draw the triangle
		GLES20.glDrawArrays(GLES20.GL_TRIANGLES, 0, vertexCount);

		// Disable vertex array
		GLES20.glDisableVertexAttribArray(mPositionHandle);
	}
}

class Square
{
	private final String vertexShaderCode =
			// This matrix member variable provides a hook to manipulate
			// the coordinates of the objects that use this vertex shader
			"uniform mat4 uMVPMatrix;" +

			"attribute vec4 vPosition;" +
			"void main() {" +
			// the matrix must be included as a modifier of gl_Position
			"  gl_Position = vPosition * uMVPMatrix;" +
			"}";

	private final String fragmentShaderCode =
			"precision mediump float;" +
			"uniform vec4 vColor;" +
			"void main() {" +
			"  gl_FragColor = vColor;" +
			"}";

	private final FloatBuffer vertexBuffer;
	private final ShortBuffer drawListBuffer;
	private final int mProgram;
	private int mPositionHandle;
	private int mColorHandle;
	private int mMVPMatrixHandle;

	// number of coordinates per vertex in this array
	static final int COORDS_PER_VERTEX = 3;
	static float squareCoords[] = { -0.5f,  0.5f, 0.0f,   // top left
																	-0.5f, -0.5f, 0.0f,   // bottom left
																	 0.5f, -0.5f, 0.0f,   // bottom right
																	 0.5f,  0.5f, 0.0f }; // top right

	private final short drawOrder[] = { 0, 1, 2, 0, 2, 3 }; // order to draw vertices

	private final int vertexStride = COORDS_PER_VERTEX * 4; // 4 bytes per vertex

	// Set color with red, green, blue and alpha (opacity) values
	float color[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	
	private final FloatBuffer textureBuffer;
	static float textureCoords[] = 
	{
		0.0f, 1.0f,		// top left
		0.0f, 0.0f,		// bottom left
		1.0f, 1.0f,		// top right
		1.0f, 0.0f		// bottom right
	}
	
	private int[] textures = new int[1];
	
	public Square()
	{
		// initialize vertex byte buffer for shape coordinates
		// (# of coordinate values * 4 bytes per float)
		ByteBuffer bb = ByteBuffer.allocateDirect(squareCoords.length * 4);
		bb.order(ByteOrder.nativeOrder());
		vertexBuffer = bb.asFloatBuffer();
		vertexBuffer.put(squareCoords);
		vertexBuffer.position(0);
		
		// reuse byte buffer for texture coordinates
		bb = ByteBuffer.allocateDirect(textureCoords.length * 4);
		bb.order(ByteOrder.nativeOrder());
		textureBuffer = bb.asFloatBuffer();
		textureBuffer.put(textureCoords);
		textureBuffer.position(0);
		
		// initialize byte buffer for the draw list
		// (# of coordinate values * 2 bytes per short)
		ByteBuffer dlb = ByteBuffer.allocateDirect(drawOrder.length * 2);
		dlb.order(ByteOrder.nativeOrder());
		drawListBuffer = dlb.asShortBuffer();
		drawListBuffer.put(drawOrder);
		drawListBuffer.position(0);

		// prepare shaders and OpenGL program
		int vertexShader = GLRenderer.loadShader(GLES20.GL_VERTEX_SHADER,
				vertexShaderCode);
		int fragmentShader = GLRenderer.loadShader(GLES20.GL_FRAGMENT_SHADER,
				fragmentShaderCode);

		// create empty OpenGL Program
		mProgram = GLES20.glCreateProgram();
		// add the vertex shader to program
		GLES20.glAttachShader(mProgram, vertexShader);
		// add the fragment shader to program
		GLES20.glAttachShader(mProgram, fragmentShader);
		// create OpenGL program executables
		GLES20.glLinkProgram(mProgram);
	}
	
	public void loadGLTexture(GL10 gl, Context context)
	{
		// loading texture
		Bitmap bitmap = BitmapFactory.decodeResource(context.getResources(),
				R.drawable.ONE);

		// generate one texture pointer
		GLES20.glGenTextures(1, this.textures, 0);
		// ...and bind it to our array
		GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, this.textures[0]);
		
		// create nearest filtered texture
		GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_NEAREST);
		GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR);
		
		// Use Android GLUtils to specify a two-dimensional texture image from our bitmap 
		GLUtils.texImage2D(GLES20.GL_TEXTURE_2D, 0, bitmap, 0);
                              
		// Clean up
		bitmap.recycle();
	}

	@Override
	public void draw(float[] mvpMatrix)
	{
		// Add program to OpenGL environment
		GLES20.glUseProgram(mProgram);

		// get handle to vertex shader's vPosition member
		mPositionHandle = GLES20.glGetAttribLocation(mProgram, "vPosition");

		// Enable a handle to the triangle vertices
		GLES20.glEnableVertexAttribArray(mPositionHandle);

		// Prepare the triangle coordinate data
		GLES20.glVertexAttribPointer(mPositionHandle, COORDS_PER_VERTEX,
				GLES20.GL_FLOAT, false, vertexStride, vertexBuffer);

		// get handle to fragment shader's vColor member
		mColorHandle = GLES20.glGetUniformLocation(mProgram, "vColor");

		// Set color for drawing the triangle
		GLES20.glUniform4fv(mColorHandle, 1, color, 0);

		// get handle to shape's transformation matrix
		mMVPMatrixHandle = GLES20.glGetUniformLocation(mProgram, "uMVPMatrix");
		GLRenderer.checkGlError("glGetUniformLocation");

		// Apply the projection and view transformation
		GLES20.glUniformMatrix4fv(mMVPMatrixHandle, 1, false, mvpMatrix, 0);
		GLRenderer.checkGlError("glUniformMatrix4fv");
		
		// bind previously generated texture
		//GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, this.textures[0]);
		//
		// http://code.google.com/p/gdc2011-android-opengl/wiki/TalkTranscript
		
		// Draw the square
		GLES20.glDrawElements(GLES20.GL_TRIANGLES, drawOrder.length,
													GLES20.GL_UNSIGNED_SHORT, drawListBuffer);

		// Disable vertex array
		GLES20.glDisableVertexAttribArray(mPositionHandle);
	}
}
