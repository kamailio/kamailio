var tree = "tree";

/**
 * Marks a link as being selected. 
 *
 * @param parentID the ID of the LI containing the link.
 */
function selectLink(parentID){
    // Clear all the classes from the a elements, and selects the target.
    var aElements = parent.tocwin.document.getElementById(tree).getElementsByTagName("a");
    var j = 0;
    for (j = 0; j < aElements.length; j++){
      if(aElements[j].parentNode.id == parentID){
        // Selected.
        aElements[j].className='selected';
      } else {
        // Unselected.
        aElements[j].className='';
      }
    }    
}

/**
 * Expands and selects a specified topic.
 *
 * @param referringTopicURL The URL of the referring topic, as string.
 * @param href The relative location of the target.
 */
function expandToTopic(referringTopicURL, href) {
  var targetAbsoluteURL = makeAbsolute(href);
  var targetAbsoluteURLArray = new Array();  
  var target;
  targetAbsoluteURLArray = targetAbsoluteURL.split("#");
  target = targetAbsoluteURLArray[0].replace("../", "");
  var idsToExpand = findIds(target);
  var toc = parent.tocwin.document;
  
   $(function(){
       if (idsToExpand != ''){
             $("#tree").dynatree("getTree").getNodeByKey(idsToExpand).focus();
       }
    });
}

function getParent(url){
  var str = "" + url;
   // Removes the last component from the path.
   url = url.substring(0, url.lastIndexOf('/'));
   return url;
}

/*
Finds all ids of parent elements of "a"'s having their hrefs ending in the target.
*/
function findIds(targetAbsoluteURL) {
  var returnedArray = new Array();
  var windowLocation = getParent(parent.tocwin.location.href);
  var toc = parent.tocwin.document.getElementById('tree');
  var aElements = toc.getElementsByTagName("a");
  var nr = aElements.length;
  var k = 0;
  for (var i = 0; i < nr; i++) {
     var linkURL = makeAbsolute(windowLocation + '/' + aElements[i].getAttribute("href"));
     if (linkURL.match(targetAbsoluteURL)) {
            returnedArray[k] = aElements[i].id;
            k++;
      }
    }
  return returnedArray;
}

/**
*  Makes absolute the input URL by stripping the .. constructs.
*/
function makeAbsolute(inputURL) {
  var url = inputURL;
  // matches a foo/../ expression
  var reParent = /[\-\w]+\/\.\.\//;
  
  // reduce all 'xyz/../' to just ''
  while (reParent.test(url)) {
    url = url.replace(reParent, "");
  }
  
  return url;
}

/**
 * Opens a page (topic) file and highlights a word from it.
 */
function openAndHighlight(page, words, linkName){
    var links = document.getElementsByTagName('a');
    for (var i = 0 ; i < links.length ; i++){
        if (links[i].id == linkName ){
            document.getElementById(linkName).className = 'otherLink';
        } else if (links[i].id.startsWith('foundLink')) {
            document.getElementById(links[i].id).className = 'foundResult';
        }
    }
    
	parent.termsToHighlight = words;
	parent.frames['contentwin'].location = page;	
}

/**
 * Hide and show div-s
 */
 
function showMenu(displayTab){
    parent.termsToHighlight = Array();
    var contentLinkText = getLocalization("Content");
    var searchLinkText = getLocalization("Search");
    var indexLinkText = getLocalization("Index");
    var tabs = document.getElementById('tocMenu').getElementsByTagName("div");
    for (var i = 0 ; i < tabs.length; i++){
        var currentTabId = tabs[i].id;
        // generates menu tabs        
        document.getElementById(currentTabId).innerHTML = '<span onclick="showMenu(\'' + currentTabId + '\')">' + eval(currentTabId + "LinkText") + '</span>';
        
        // show selected block
        selectedBlock = displayTab + "Block";
        if (currentTabId == displayTab){
            document.getElementById(selectedBlock).style.display = "block";
            $('#' + currentTabId).addClass('selected');
        } else  {
            document.getElementById(currentTabId + 'Block').style.display = "none";
            $('#' + currentTabId).removeClass('selected');
         }   
    }
	if (displayTab == 'content') {
        var pathPrefix = parent.location.pathname;
        var expandPage = getHTMLPage2Expand(parent.contentwin.location.pathname);
        if(expandPage){
            expandPage = expandPage.replace(pathPrefix, "");
            expandToTopic(window.location.href, expandPage);
        }
    }
   
    if (displayTab == 'search') {
        $('.textToSearch').focus();
    }
    if (displayTab == 'index') {
        $('#id_search').focus();
    }
  //  $('*', window.parent.contentwin.document).unhighlight();
} 

 
function hideDiv(hiddenDiv,showedDiv){   
    parent.termsToHighlight = Array();
    document.getElementById(hiddenDiv).style.display = "none";
    document.getElementById(showedDiv).style.display = "block";
    var contentLinkText = getLocalization("Content");
    var searchLinkText = getLocalization("Search");
    
	if (hiddenDiv == 'searchDiv') {
		document.getElementById('divContent').innerHTML = '<font class="normalLink">' + contentLinkText + '</font>';
		document.getElementById('divSearch').innerHTML = '<a href="javascript:void(0);" class="activeLink" id="searchLink" onclick="hideDiv(\'displayContentDiv\',\'searchDiv\')">' + searchLinkText + '</a>';
        var pathPrefix = parent.location.pathname;
        var expandPage = getHTMLPage2Expand(parent.contentwin.location.pathname);
        expandPage = expandPage.replace(pathPrefix, "");
        expandToTopic(window.location.href, expandPage);
    } else {
		document.getElementById('divContent').innerHTML = '<a href="javascript:void(0);" class="activeLink" id="contentLink" onclick="hideDiv(\'searchDiv\',\'displayContentDiv\')">' + contentLinkText + '</a>';
		document.getElementById('divSearch').innerHTML = '<font class="normalLink">' + searchLinkText + '</font>';
	}
    
  //  $('*', window.parent.contentwin.document).unhighlight();
}

/**
 *  Get the localized string for the specified key.
 */
function getLocalization(localizationKey) {
	if (localization[localizationKey]){
		return localization[localizationKey];
	}else{
		return localizationKey;
	}
}


    function getHTMLPage2Expand(url){
        currentPage =url;
    if(typeof url != 'undefined'){
      var page = url.substr(1);
      //var page = url;
      currentPage = page;
      page = parent.location.search.substr(1).split("&");
      for (x in page) {
        var q;
        q = page[x].split("=");;
        if(q[0] == 'q'){
         currentPage = q[1];
        }
      }
      }
      return currentPage;
    }