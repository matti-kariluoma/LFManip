// Library of utility functions for LFManip http://github.com/matti-kariluoma/LFManip
//
// Matti Kariluoma Jan 2013 <matti@kariluo.ma>

var lfmanip = {
	
	filenames: new Array(),
	
	render: function(){},
	
	init: function(config)
		{
			lfmanip.filenames = config.filenames;
			
			lfmanip.render = config.render;
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
			for (var i=0; i<lfmanip.filenames.length; i++)
			{
				var image = new Image();
				image.src = lfmanip.filenames[i];
				unloadedImages.push(image);
			}
			
			for (i=0; i<unloadedImages.length; i++)
			{
				// wait for the images to load before continuing
				unloadedImages[i].onload = lfmanip.handleImageLoad(unloadedImages[i], i);
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
				lfmanip.render();
				requestAnimFrame(canUpdate);
			}
			
			// begin the loop
			canUpdate();
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
	
	
};
