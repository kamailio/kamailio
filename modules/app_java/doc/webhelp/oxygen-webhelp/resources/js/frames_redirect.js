var pageTo =  parent.location.search;
var redirectPageTo ="";
if (pageTo){
 redirectPageTo = pageTo.substring(3);
}

/**
 * Redirects to frames/no frames version of the manual. 
 *
 * @param currentUrl the link of the page that is redirected to the frames version.
 */
function redirectFrames(currentUrl){
  if (parent.window.location.pathname != window.location.pathname){
    //No Frames
    parent.window.location = "http://" + location.hostname + currentUrl;  
  } else {
    //With Frames
    if(/MSIE (\d+\.\d+);/.test(navigator.userAgent) && location.hostname == '' && currentUrl.search("/") == '0'){
        currentUrl = currentUrl.substr(1);
    }
    window.location = prefix + "?q=" + currentUrl;
  }
}

function getPath(currentUrl){
    //With Frames
    if(/MSIE (\d+\.\d+);/.test(navigator.userAgent) && location.hostname == '' && currentUrl.search("/") == '0'){
        currentUrl = currentUrl.substr(1);
    }
    path = prefix + "?q=" + currentUrl;
    return path;
}
  
/**
 * Redirects to the frames version of the manual if a parameter is found in url. 
 *
 * @param toc - if true redirects the page to the frames version 
 */
function redirectToToc(url){  
   var page = url.substr(1);
   var x;
   if (page != ""){
     page = page.split("&");
     for (x in page) {
        if(page[x] == 'toc=true'){
         redirectFrames(window.location.pathname); 
        }
     }
   }
} 

  