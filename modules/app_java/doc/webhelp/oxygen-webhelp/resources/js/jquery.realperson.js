/* http://keith-wood.name/realPerson.html
   Real Person Form Submission for jQuery v1.0.1.
   Written by Keith Wood (kwood{at}iinet.com.au) June 2009.
   Dual licensed under the GPL (http://dev.jquery.com/browser/trunk/jquery/GPL-LICENSE.txt) and 
   MIT (http://dev.jquery.com/browser/trunk/jquery/MIT-LICENSE.txt) licenses. 
   Please attribute the author if you use it. */

(function($) { // Hide scope, no $ conflict

var PROP_NAME = 'realPerson';

/* Real person manager. */
function RealPerson() {
	this._defaults = {
		length: 6, // Number of characters to use
		includeNumbers: false, // True to use numbers as well as letters
		regenerate: 'Click to change', // Instruction text to regenerate
		hashName: '{n}Hash' // Name of the hash value field to compare with,
			// use {n} to substitute with the original field name
	};
}

var CHARS = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789';
var DOTS = [
	['   *   ', '  * *  ', '  * *  ', ' *   * ', ' ***** ', '*     *', '*     *'],
	['****** ', '*     *', '*     *', '****** ', '*     *', '*     *', '****** '],
	[' ***** ', '*     *', '*      ', '*      ', '*      ', '*     *', ' ***** '],
	['****** ', '*     *', '*     *', '*     *', '*     *', '*     *', '****** '],
	['*******', '*      ', '*      ', '****   ', '*      ', '*      ', '*******'],
	['*******', '*      ', '*      ', '****   ', '*      ', '*      ', '*      '],
	[' ***** ', '*     *', '*      ', '*      ', '*   ***', '*     *', ' ***** '],
	['*     *', '*     *', '*     *', '*******', '*     *', '*     *', '*     *'],
	['*******', '   *   ', '   *   ', '   *   ', '   *   ', '   *   ', '*******'],
	['      *', '      *', '      *', '      *', '      *', '*     *', ' ***** '],
	['*     *', '*   ** ', '* **   ', '**     ', '* **   ', '*   ** ', '*     *'],
	['*      ', '*      ', '*      ', '*      ', '*      ', '*      ', '*******'],
	['*     *', '**   **', '* * * *', '*  *  *', '*     *', '*     *', '*     *'],
	['*     *', '**    *', '* *   *', '*  *  *', '*   * *', '*    **', '*     *'],
	[' ***** ', '*     *', '*     *', '*     *', '*     *', '*     *', ' ***** '],
	['****** ', '*     *', '*     *', '****** ', '*      ', '*      ', '*      '],
	[' ***** ', '*     *', '*     *', '*     *', '*   * *', '*    * ', ' **** *'],
	['****** ', '*     *', '*     *', '****** ', '*   *  ', '*    * ', '*     *'],
	[' ***** ', '*     *', '*      ', ' ***** ', '      *', '*     *', ' ***** '],
	['*******', '   *   ', '   *   ', '   *   ', '   *   ', '   *   ', '   *   '],
	['*     *', '*     *', '*     *', '*     *', '*     *', '*     *', ' ***** '],
	['*     *', '*     *', ' *   * ', ' *   * ', '  * *  ', '  * *  ', '   *   '],
	['*     *', '*     *', '*     *', '*  *  *', '* * * *', '**   **', '*     *'],
	['*     *', ' *   * ', '  * *  ', '   *   ', '  * *  ', ' *   * ', '*     *'],
	['*     *', ' *   * ', '  * *  ', '   *   ', '   *   ', '   *   ', '   *   '],
	['*******', '     * ', '    *  ', '   *   ', '  *    ', ' *     ', '*******'],
	['  ***  ', ' *   * ', '*     *', '*     *', '*     *', ' *   * ', '  ***  '],
	['   *   ', '  **   ', ' * *   ', '   *   ', '   *   ', '   *   ', '*******'],
	[' ***** ', '*     *', '      *', '     * ', '   **  ', ' **    ', '*******'],
	[' ***** ', '*     *', '      *', '    ** ', '      *', '*     *', ' ***** '],
	['    *  ', '   **  ', '  * *  ', ' *  *  ', '*******', '    *  ', '    *  '],
	['*******', '*      ', '****** ', '      *', '      *', '*     *', ' ***** '],
	['  **** ', ' *     ', '*      ', '****** ', '*     *', '*     *', ' ***** '],
	['*******', '     * ', '    *  ', '   *   ', '  *    ', ' *     ', '*      '],
	[' ***** ', '*     *', '*     *', ' ***** ', '*     *', '*     *', ' ***** '],
	[' ***** ', '*     *', '*     *', ' ******', '      *', '     * ', ' ****  ']];

$.extend(RealPerson.prototype, {
	/* Class name added to elements to indicate already configured with real person. */
	markerClassName: 'hasRealPerson',

	/* Override the default settings for all real person instances.
	   @param  settings  (object) the new settings to use as defaults
	   @return  (RealPerson) this object */
	setDefaults: function(settings) {
		$.extend(this._defaults, settings || {});
		return this;
	},

	/* Attach the real person functionality to an input field.
	   @param  target    (element) the control to affect
	   @param  settings  (object) the custom options for this instance */
	_attachRealPerson: function(target, settings) {
		target = $(target);
		if (target.hasClass(this.markerClassName)) {
			return;
		}
		target.addClass(this.markerClassName);
		var inst = {settings: $.extend({}, this._defaults)};
		$.data(target[0], PROP_NAME, inst);
		this._changeRealPerson(target, settings);
	},

	/* Reconfigure the settings for a real person control.
	   @param  target    (element) the control to affect
	   @param  settings  (object) the new options for this instance or
	                     (string) an individual property name
	   @param  value     (any) the individual property value (omit if settings is an object) */
	_changeRealPerson: function(target, settings, value) {
		target = $(target);
		if (!target.hasClass(this.markerClassName)) {
			return;
		}
		settings = settings || {};
		if (typeof settings == 'string') {
			var name = settings;
			settings = {};
			settings[name] = value;
		}
		var inst = $.data(target[0], PROP_NAME);
		$.extend(inst.settings, settings);
		target.prevAll('.realperson-challenge,.realperson-hash').remove().end().
			before(this._generateHTML(target, inst));
	},

	/* Generate the additional content for this control.
	   @param  target  (jQuery) the input field
	   @param  inst    (object) the current instance settings
	   @return  (string) the additional content */
	_generateHTML: function(target, inst) {
		var text = '';
		for (var i = 0; i < inst.settings.length; i++) {
			text += CHARS.charAt(Math.floor(Math.random() *
				(inst.settings.includeNumbers ? 36 : 26)));
		}
		var html = '<div class="realperson-challenge"><div class="realperson-text">';
		for (var i = 0; i < DOTS[0].length; i++) {
			for (var j = 0; j < text.length; j++) {
				html += DOTS[CHARS.indexOf(text.charAt(j))][i].replace(/ /g, '&nbsp;') +
					'&nbsp;&nbsp;';
			}
			html += '<br>';
		}
		html += '</div><div class="realperson-regen">' + inst.settings.regenerate +
			'</div></div><input type="hidden" class="realperson-hash" name="' +
			inst.settings.hashName.replace(/\{n\}/, target.attr('name')) +
			'" value="' + this._hash(text) + '">';
		return html;
	},

	/* Remove the real person functionality from a control.
	   @param  target  (element) the control to affect */
	_destroyRealPerson: function(target) {
		target = $(target);
		if (!target.hasClass(this.markerClassName)) {
			return;
		}
		target.removeClass(this.markerClassName).
			prevAll('.realperson-challenge,.realperson-hash').remove();
		$.removeData(target[0], PROP_NAME);
	},

	/* Compute a hash value for the given text.
	   @param  value  (string) the text to hash
	   @return  the corresponding hash value */
	_hash: function(value) {
		var hash = 5381;
		for (var i = 0; i < value.length; i++) {
			hash = ((hash << 5) + hash) + value.charCodeAt(i);
		}
		return hash;
	}
});

/* Attach the real person functionality to a jQuery selection.
   @param  command  (string) the command to run (optional, default 'attach')
   @param  options  (object) the new settings to use for these instances (optional)
   @return  (jQuery) for chaining further calls */
$.fn.realperson = function(options) {
	var otherArgs = Array.prototype.slice.call(arguments, 1);
	return this.each(function() {
		if (typeof options == 'string') {
			$.realperson['_' + options + 'RealPerson'].
				apply($.realperson, [this].concat(otherArgs));
		}
		else {
			$.realperson._attachRealPerson(this, options || {});
		}
	});
};

/* Initialise the real person functionality. */
$.realperson = new RealPerson(); // singleton instance

$('.realperson-challenge').live('click', function() {
	$(this).next().next().realperson('change');
});

})(jQuery);
