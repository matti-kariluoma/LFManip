// Library of utility functions for LFManip http://github.com/matti-kariluoma/LFManip
//
// Matti Kariluoma Jan 2013 <matti@kariluo.ma>

var lfmanip = {
	
	init: function(config)
		{
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
			//$('#theCanvas').height($(document).height());
			$('#theCanvas').width($(document).width());
		},

	start: function(renderFn)
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
				renderFn();
				requestAnimFrame(canUpdate);
			}
			
			// begin the loop
			canUpdate();
		},
};
