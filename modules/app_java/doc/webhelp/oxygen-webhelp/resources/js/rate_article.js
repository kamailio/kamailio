/**
 * Reads a cookie 
 *
 * @param a the cookie name.
 * @return b the cookie value of the 'a' cookie.
 */
function readCookie(a) {
  var b = "";
  a = a + "=";
  if(document.cookie.length > 0) {
    offset = document.cookie.indexOf(a);
    if(offset != -1) {
      offset += a.length;
      end = document.cookie.indexOf(";", offset);
      if(end == -1)end = document.cookie.length;
      b = unescape(document.cookie.substring(offset, end))
    }
  }return b
}

/**
 * Write in a cookie the rate star value 
 *
 * @param a the cookie name.
 * @param b the cookie value of the 'a' cookie.
 * @param d the cookie expiration time.
 */
function writeStar(a, b, d, f) {document.cookie = a + "=" + b + "; path = " + f; }

/**
 * Set the rate (how many stars and the title description) and shows the feedback textarea  
 *
 * @param star - how many stars the user selected.
 * @param title - the selected stars description: e.g. 'Somewhat helpful'.
 */
function setRate(star, title){
  var x = document.getElementsByTagName('a');
  // set cookie
  writeStar('rateUGO2', star + ' -> ' + title,1440 ,'');
  for (i = 0; i < x.length; i++) {
    if (x[i].className.match('show_star')) {      
      x[i].className = "";
    }          
  }
  document.getElementById('rate_comment').className = 'show';
  document.getElementById(star).className = 'show_star';
}

$(function() {
  $('textarea#feedback').focus(function(){
    $(this).css({backgroundColor:"#fff"});
  });
  $('textarea#feedback').blur(function(){
    $(this).css({backgroundColor:"#fff"});
  });

  $(".button").click(function() {
		// process form
		var feedback = $("textarea#feedback").val();
		var dataString = '&feedback=' + feedback + '&page=' + window.location.href+ '&star=' + readCookie('rateUGO2');
		var pageRateFile = ratingFile;
		$.ajax({
      type: "POST",
      url: pageRateFile,
      data: dataString,
      success: function() {
        $('div#rate_stars').css({display:"none"});
        $('#rate_comment').html("<div class='rate_response'>Thank you for your feedback!</div>");
        $('#rate_response').html("")
        .hide()
        .fadeIn(1000, function() {
          $('#message').append("<img id='checkmark' src='../img/check.png' />");
        });
      }
     });
    return false;
	});
});

