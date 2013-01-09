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
import android.os.Bundle;

import com.mattikariluoma.lfmanip.GLSurfaceView;

public class LFManip extends Activity
{
	private GLSurfaceView mGLView;

	@Override
	public void onCreate(Bundle savedInstanceState) 
	{
		super.onCreate(savedInstanceState);

		// create a GLSurfaceView, set it as the ContentView
		mGLView = new GLSurfaceView(this);
		setContentView(mGLView);
	}

	@Override
	protected void onPause() 
	{
		super.onPause();
		// deallocate large textures

		// pause rendering
		mGLView.onPause();
	}

	@Override
	protected void onResume() 
	{
		super.onResume();
		// reallocate any textures

		// resume rendering
		mGLView.onResume();
	}
}
