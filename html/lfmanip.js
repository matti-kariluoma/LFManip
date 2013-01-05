// Library of utility functions for LFManip http://github.com/matti-kariluoma/LFManip
//
// Matti Kariluoma Jan 2013 <matti@kariluo.ma>

var lfmanip = {
	
	filenames: new Array(),
	
	dx: new Array(),
	
	dy: new Array(),
	
	render: function(){},
	
	canvas: $('canvas'), // defaults to all canvases.
	
	wheel: function(){},
	
	click: function(){},
	
	init: function(config)
		{
			lfmanip.filenames = config.filenames;
			lfmanip.dx = config.dx;
			lfmanip.dy = config.dy;
			lfmanip.render = config.render;
			lfmanip.canvas = config.canvas;
			lfmanip.wheel = config.wheel;
			lfmanip.click = config.click;
			
			// check that dx.length == dy.length == filenames.length
			if (lfmanip.filenames.length != lfmanip.dx.length || lfmanip.dx.length != lfmanip.dy.length)
			{
				for (i=0; i<lfmanip.filenames.length; i++)
				{
					if (!lfmanip.dx[i])
					{
						lfmanip.dx.push(0.0);
					}
					if (!lfmanip.dy[i])
					{
						lfmanip.dy.push(0.0);
					}
				}
			}
			
			// set listener to mousewheel event
			lfmanip.canvas.mousewheel(lfmanip.wheel);
			
			// set listener to click event
			lfmanip.canvas.click(lfmanip.click);
			
			// resize the canvas now
			lfmanip.resizeCanvas();

			// set listener to resize as browser resizes
			$(
				function()
				{
					$(window).resize(
						function()
						{
							lfmanip.resizeCanvas();
						}
					);
				}
			);
		},
	
	resizeCanvas: function()
		{
			var canvas = $('#theCanvas');
			var width = canvas[0].offsetWidth;
			var height = canvas[0].offsetHeight;
			
			canvas.attr('width', width);
			canvas.attr('height', height);
		},
	
	started: false,
	
	start: function()
		{
			var unloadedImages = new Array();
			for (i=0; i<lfmanip.filenames.length; i++)
			{
				var image = new Image();
				// wait for the images to load before continuing
				image.onload = lfmanip.handleImageLoad(image, i);
				image.src = lfmanip.filenames[i];
				image.dx = lfmanip.dx[i];
				image.dy = lfmanip.dy[i];
				unloadedImages.push(image);
			}
			
			// handleImageLoad() will eventually call loop()
		},
	
	loop: function()
		{
			// cross-browser selector from http://stackoverflow.com/questions/5864073/game-loop-requestanimframe-javascript-canvas
			var requestAnimFrame = (
				// define a selector function, then immediately call it
				function() 
				{
					return window.requestAnimationFrame
						|| window.webkitRequestAnimationFrame
						|| window.mozRequestAnimationFrame
						|| window.oRequestAnimationFrame
						|| window.msRequestAnimationFrame
						|| function(callback, element) 
							{
								window.setTimeout(callback, 1000/60);
							};
				}
			)();
			
			var canUpdate = function()
			{
				if (lfmanip.started)
				{
					lfmanip.render();
					requestAnimFrame(canUpdate);
				}
			}
			
			// begin the loop
			canUpdate();
		},
	
	stop: function()
		{
			lfmanip.started = false;
			lfmanip.images = new Array();
		},
	
	images: new Array(),
	
	handleImageLoad: function(image, index)
		{
			// bind the img and index variables, then return a function
			return function()
			{
				lfmanip.registerImage(image, index);
				if (!lfmanip.started)
				{
					lfmanip.started = true;
					lfmanip.loop();
				}
			};
		},
	
	registerImage: function(img, index)
		{
			lfmanip.images.splice(index, 0, img)
		},
		
	getImage: function(index)
		{
			var img = null;

			if (lfmanip.images.length >= index)
			{
				img = lfmanip.images[index];
			}
			else if (lfmanip.images.length > 0)
			{
				img = lfmanip.images[0];
			}
			else
			{
				img = new Image();
			}

			return img;
		},
	
	numImages: function()
		{
			var num = 0
			if (lfmanip.images && lfmanip.images.length)
			{
				num = lfmanip.images.length
			}
			return num;
		},
	
};
