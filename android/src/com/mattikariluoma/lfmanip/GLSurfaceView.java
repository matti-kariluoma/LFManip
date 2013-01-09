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

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.view.MotionEvent;

import com.mattikariluoma.lfmanip.GLRenderer;

class GLSurfaceView extends android.opengl.GLSurfaceView 
{
	private final GLRenderer mRenderer;
	private final float TOUCH_SCALE_FACTOR = 180.0f / 320;
	private float mPreviousX;
	private float mPreviousY;

	public GLSurfaceView(Context context) 
	{
		super(context);

		// Create an OpenGL ES 2.0 context.
		setEGLContextClientVersion(2);

		// Set the Renderer for drawing on the GLSurfaceView
		mRenderer = new GLRenderer();
		setRenderer(mRenderer);

		// Render the view only when there is a change in the drawing data
		setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);
	}


	@Override
	public boolean onTouchEvent(MotionEvent e) 
	{
		// MotionEvent reports input details from the touch screen
		// and other input controls. In this case, you are only
		// interested in events where the touch position changed.

		float x = e.getX();
		float y = e.getY();

		switch (e.getAction()) 
		{
			case MotionEvent.ACTION_MOVE:
				float dx = x - mPreviousX;
				float dy = y - mPreviousY;

				// reverse direction of rotation above the mid-line
				if (y > getHeight() / 2) 
				{
					dx = dx * -1 ;
				}

				// reverse direction of rotation to left of the mid-line
				if (x < getWidth() / 2) 
				{
					dy = dy * -1 ;
				}

				mRenderer.mAngle += (dx + dy) * TOUCH_SCALE_FACTOR;  // = 180.0f / 320
				requestRender();
		}

		mPreviousX = x;
		mPreviousY = y;
		return true;
	}
}

