/*----------------------------------------------------------------------------
 * JavaScript for webhelp search
 *----------------------------------------------------------------------------
 This file is part of the webhelpsearch plugin for DocBook WebHelp
 Copyright (c) 2007-2008 NexWave Solutions All Rights Reserved.
 www.nexwave.biz Nadege Quaine
 http://kasunbg.blogspot.com/ Kasun Gajasinghe
 */

//string initialization
var htmlfileList = "htmlFileList.js";
var htmlfileinfoList = "htmlFileInfoList.js";
var useCJKTokenizing = false;

//-------------------------OXYGEN PATCH START-------------------------
// The file was generated from eXml-webHelp ant file
// Any modification of this file will be made in eXml-webHelp Project

var w = new Object();
var scoring = new Object();

var searchTextField = '';
var no = 0;
var noWords = 0;
var partialSearch1 = "There is no page containing all the search terms.";
var partialSearch2 = "Partial results:";
var warningMsg = '<div style="padding: 5px;margin-right:5px;;background-color:#FFFF00;">';
warningMsg+='<b>Please note that due to security settings, Google Chrome does not highlight';
warningMsg+=' the search results in the right frame.</b><br>';
warningMsg+='This happens only when the WebHelp files are loaded from the local file system.<br>';
warningMsg+='Workarounds:';
warningMsg+='<ul>';
warningMsg+='<li>Try using another web browser.</li>';
warningMsg+='<li>Deploy the WebHelp files on a web server.</li>';
warningMsg+='</div>';
txt_filesfound = 'Results';
txt_enter_at_least_1_char = "You must enter at least one character.";
txt_enter_more_than_10_words = "Only first 10 words will be processed.";
txt_browser_not_supported = "Your browser is not supported. Use of Mozilla Firefox is recommended.";
txt_please_wait = "Please wait. Search in progress...";
txt_results_for = "Results for:";
//-------------------------OXYGEN PATCH END-------------------------

/* Cette fonction verifie la validite de la recherche entrre par l utilisateur */
function Verifie(ditaSearch_Form) {

    // Check browser compatibitily
    if (navigator.userAgent.indexOf("Konquerer") > -1) {

        alert(getLocalization(txt_browser_not_supported));
        return;
    }

    //-------------------------OXYGEN PATCH START-------------------------
    /*
    var expressionInput = document.ditaSearch_Form.textToSearch.value
    */
    searchTextField = trim(document.searchForm.textToSearch.value);
    var expressionInput = searchTextField;  

    /*
    //Set a cookie to store the searched keywords
    $.cookie('textToSearch', expressionInput);
    */
    //-------------------------OXYGEN PATCH END-------------------------

    if (expressionInput.length < 1) {
        // expression is invalid
        alert(getLocalization(txt_enter_at_least_1_char));
        // reactive la fenetre de search (utile car cadres)

        //-------------------------OXYGEN PATCH START-------------------------
        /*
        document.ditaSearch_Form.textToSearch.focus();
        */
        document.searchForm.textToSearch.focus();
        //-------------------------OXYGEN PATCH END-------------------------
    }
    else {
        //-------------------------OXYGEN PATCH START-------------------------
       // OXYGEN PATCH START - EXM-20996 - split by " ", ".", ":", "-"
    var splitSpace = searchTextField.split(" ");
    
    /*
       var splitWords = [];
        for (var i = 0 ; i < splitSpace.length ; i++) {
          var splitDot = splitSpace[i].split(".");
          for (var i1 = 0; i1 < splitDot.length; i1++) {
               var splitColon = splitDot[i1].split(":");
            for (var i2 = 0; i2 < splitColon.length; i2++) {
                var splitDash = splitColon[i2].split("-");
                 for (var i3 = 0; i3 < splitDash.length; i3++) {
                     if (splitDash[i3].split("").length > 0) {
                           splitWords.push(splitDash[i3]);
                       }
                 }
            }
          }
       }
       noWords = splitWords;
       */
       
       noWords = splitSpace;

       // OXYGEN PATCH END - EXM-20996 - split by " ", ".", ":", "-"
    
      if (noWords.length > 9) {
          // Allow to search maximum 10 words
          alert(getLocalization(txt_enter_more_than_10_words));
          expressionInput = '';
           for (var x = 0 ; x < 10 ; x++){
               expressionInput = expressionInput + " " + noWords[x]; 
          }
          Effectuer_recherche(expressionInput);
           document.searchForm.textToSearch.focus();
      } else {
          // Effectuer la recherche
             // OXYGEN PATCH START - EXM-20996
             expressionInput = '';
          for (var x = 0 ; x < noWords.length ; x++) {
                 expressionInput = expressionInput + " " + noWords[x]; 
             }
             // OXYGEN PATCH END - EXM-20996
             
             Effectuer_recherche(expressionInput);
          // reactive la fenetre de search (utile car cadres)
          /*
          document.ditaSearch_Form.textToSearch.focus();
          */
          document.searchForm.textToSearch.focus();        
          //-------------------------OXYGEN PATCH END-------------------------
      }
    }
}

var stemQueryMap = new Array();  // A hashtable which maps stems to query words

/* This function parses the search expression, loads the indices and displays the results*/
function Effectuer_recherche(expressionInput) {

    /* Display a waiting message */
    //DisplayWaitingMessage();

    /*data initialisation*/
    var searchFor = "";       // expression en lowercase et sans les caracte    res speciaux
    //w = new Object();  // hashtable, key=word, value = list of the index of the html files
    scriptLetterTab = new Scriptfirstchar(); // Array containing the first letter of each word to look for
    var wordsList = new Array(); // Array with the words to look for
    var finalWordsList = new Array(); // Array with the words to look for after removing spaces
    var linkTab = new Array();
    var fileAndWordList = new Array();
    var txt_wordsnotfound = "";


    /*nqu: expressionInput, la recherche est lower cased, plus remplacement des char speciaux*/
    /*
      %21   !
      %2C   ,
      %3A   :
      %3B   ;
    
    */
		/*  OXYGEN PATCH START - EXM-20414 */
    searchFor = expressionInput.toLowerCase().replace(/<\//g, "_st_").replace(/\$_/g, "_di_").replace(/%2C|%3B|%21|%3A|@|\/|\*/g, " ").replace(/(%20)+/g, " ").replace(/_st_/g, "</").replace(/_di_/g, "%24_");
		/*  OXYGEN PATCH END - EXM-20414 */

    searchFor = searchFor.replace(/  +/g, " ");
    searchFor = searchFor.replace(/ $/, "").replace(/^ /, "");

    wordsList = searchFor.split(" ");
    wordsList.sort();

    //set the tokenizing method
    if(typeof indexerLanguage != "undefined" && (indexerLanguage=="zh" || indexerLanguage=="ja" ||indexerLanguage=="ko")){
        useCJKTokenizing=true;
    } else {
        useCJKTokenizing=false;
    }
    //If Lucene CJKTokenizer was used as the indexer, then useCJKTokenizing will be true. Else, do normal tokenizing.
    // 2-gram tokenizinghappens in CJKTokenizing, 
    // OXYGEN PATCH START. If doStem then make tokenize with Stemmer
    var finalArray;
    if (doStem){
    // OXYGEN PATCH END.
      if(useCJKTokenizing){
          finalWordsList = cjkTokenize(wordsList);
          finalArray = finalWordsList;
      } else { 
          finalWordsList = tokenize(wordsList);
      }
    } else if(useCJKTokenizing){
          finalWordsList = cjkTokenize(wordsList);
          finalArray = finalWordsList;
         } 
    if(!useCJKTokenizing){
                //load the scripts with the indices: the following lines do not work on the server. To be corrected
              /*if (IEBrowser) {
               scriptsarray = loadTheIndexScripts (scriptLetterTab);
               } */
          
              /**
               * Compare with the indexed words (in the w[] array), and push words that are in it to tempTab.
               */
              var tempTab = new Array();
            
            var splitedValues = expressionInput.split(" ");
            finalWordsList = finalWordsList.concat(splitedValues);
            finalArray = finalWordsList;
            finalArray = removeDuplicate(finalArray);
              // OXYGEN PATCH START.
              var wordsArray = '';
              // OXYGEN PATCH END.
              for (var t in finalWordsList) {  
              // OXYGEN PATCH START.
              if (doStem){
              // OXYGEN PATCH END.
                    if (w[finalWordsList[t].toString()] == undefined) {
                        txt_wordsnotfound += finalWordsList[t] + " ";
                    } else {
                        tempTab.push(finalWordsList[t]);
                    }
              // OXYGEN PATCH START.
                } else {
                  var searchedValue = finalWordsList[t].toString();
                  if (wordsStartsWith(searchedValue) != undefined){
                    wordsArray+=wordsStartsWith(searchedValue);
                  }
                }
              // OXYGEN PATCH END.
              }
              // OXYGEN PATCH START.
              wordsArray = wordsArray.substr(0, wordsArray.length - 1);    
            if (!doStem){   
              finalWordsList = wordsArray.split(",");
            } else {
                finalWordsList = tempTab;   
            }
              // OXYGEN PATCH END.
          
              //-------------------------OXYGEN PATCH START-----------------------
              txt_wordsnotfound = expressionInput;
            finalWordsList = removeDuplicate(finalWordsList);
              //-------------------------OXYGEN PATCH END-------------------------
   }

 
    if (finalWordsList.length) {
      //search 'and' and 'or' one time
      fileAndWordList = SortResults(finalWordsList);
      //-------------------------OXYGEN PATCH START-----------------------
      if (fileAndWordList == undefined) {
          var cpt = 0;
      } else {
          var cpt = fileAndWordList.length;
          var maxNumberOfWords = fileAndWordList[0][0].motsnb;
      }
    if (cpt > 0) {
		var searchedWords = noWords.length;
		var foundedWords  = fileAndWordList[0][0].motslisteDisplay.split(",").length;
		//console.info("search : " + noWords.length + "   found : " + fileAndWordList[0][0].motslisteDisplay.split(",").length);
		if (searchedWords != foundedWords) {
		  linkTab.push("<font class=\"highlightText\">" + getLocalization(partialSearch1) + "<br>" + getLocalization(partialSearch2) + "</font>");
		}
    }
    
      //-------------------------OXYGEN PATCH END-----------------------
      for (var i = 0; i < cpt; i++) {
      //-------------------------OXYGEN PATCH START-----------------------
      var hundredProcent = fileAndWordList[i][0].scoring + 100 * fileAndWordList[i][0].motsnb;
      var ttScore_first = fileAndWordList[i][0].scoring;
      var numberOfWords = fileAndWordList[i][0].motsnb;
      //-------------------------OXYGEN PATCH END-----------------------
            if (fileAndWordList[i] != undefined) {
                linkTab.push("<p>" + getLocalization(txt_results_for) + " " + "<span class=\"searchExpression\">" + fileAndWordList[i][0].motslisteDisplay + "</span>" + "</p>");

                linkTab.push("<ul class='searchresult'>");
                for (t in fileAndWordList[i]) {
                    //linkTab.push("<li><a href=\"../"+fl[fileAndWordList[i][t].filenb]+"\">"+fl[fileAndWordList[i][t].filenb]+"</a></li>");
            //-------------------------OXYGEN PATCH START-----------------------                    
                    var ttInfo = fileAndWordList[i][t].filenb;
                    // Get scoring
                    var ttScore = fileAndWordList[i][t].scoring;
                    var tempInfo = fil[ttInfo];
            //-------------------------OXYGEN PATCH END-----------------------
                    var pos1 = tempInfo.indexOf("@@@");
                    var pos2 = tempInfo.lastIndexOf("@@@");
                    var tempPath = tempInfo.substring(0, pos1);
                    var tempTitle = tempInfo.substring(pos1 + 3, pos2);
                    var tempShortdesc = tempInfo.substring(pos2 + 3, tempInfo.length);

                    //-------------------------OXYGEN PATCH START-------------------------
                    // toc.html will not be displayed on search result
                    if (tempPath == 'toc.html'){
                        continue;
                    }
                    /*
                    //file:///home/kasun/docbook/WEBHELP/webhelp-draft-output-format-idea/src/main/resources/web/webhelp/installation.html
                    var linkString = "<li><a href=" + tempPath + ">" + tempTitle + "</a>";
                    // var linkString = "<li><a href=\"installation.html\">" + tempTitle + "</a>";
                    */
                    var split = fileAndWordList[i][t].motsliste.split(",");
                    // var splitedValues = expressionInput.split(" ");
          // var finalArray = split.concat(splitedValues);          
          
                    arrayString = 'Array(';
                    for(var x in finalArray){
                      if (finalArray[x].length > 2 || useCJKTokenizing){
                        arrayString+= "'" + finalArray[x] + "',";
                      } 
                    }
                    arrayString = arrayString.substring(0,arrayString.length - 1) + ")";
					/*  OXYGEN PATCH START - EXM-20414 */
					arrayString = arrayString.replace(".", "\\\\.");
					/*  OXYGEN PATCH END - EXM-20414 */
                    var idLink = 'foundLink' + no;
                    var link = 'openAndHighlight(\'' + tempPath + '\', ' + arrayString + ', \'' + idLink + '\')';
                    var linkString = '<li><a id="' + idLink + '" href="' + tempPath + '" class="foundResult" onclick="'+link+'">' + tempTitle + '</a>';
                    var starWidth = (ttScore * 100/ hundredProcent)/(ttScore_first/hundredProcent) * (numberOfWords/maxNumberOfWords);
                    starWidth = starWidth < 10 ? (starWidth + 5) : starWidth;
                    // Keep the 5 stars format
                    if (starWidth > 85){
            starWidth = 85;
          }
          /*
          var noFullStars = Math.ceil(starWidth/17);
          var fullStar  = "curr";
          var emptyStar = "";
          if (starWidth % 17 == 0){
            // am stea plina
            
          } else {
            
          }
          console.info(noFullStars);
          */
                    // Also check if we have a valid description
                    if ((tempShortdesc != "null" && tempShortdesc != '...')) {
                    //-------------------------OXYGEN PATCH END-------------------------
                        linkString += "\n<div class=\"shortdesclink\">" + tempShortdesc + "</div>";
                    }
                    linkString += "</li>";
                    //-------------------------OXYGEN PATCH START-------------------------
                    // Add rating values for scoring at the list of matches 
          linkString += "<div id=\"rightDiv\">";
          linkString += "<div id=\"star\">";
          //linkString += "<div style=\"color: rgb(136, 136, 136);\" id=\"starUser0\" class=\"user\">" 
          //        + ((ttScore * 100/ hundredProcent)/(ttScore_first/hundredProcent)) * 1 + "</div>";
                  linkString += "<ul id=\"star0\" class=\"star\">";
          linkString += "<li id=\"starCur0\" class=\"curr\" style=\"width: " + starWidth + "px;\"></li>";
                  linkString += "</ul>";
                  
                  linkString += "<br style=\"clear: both;\">";
                  linkString += "</div>";
          linkString += "</div>";
                    //linkString += '<b>Rating: ' + ttScore + '</b>';
                    //-------------------------OXYGEN PATCH END-------------------------                       
                    linkTab.push(linkString);
                    no++;
                }
                linkTab.push("</ul>");
            }
        }
    }

    var results = "";
    if (linkTab.length > 0) { 
        /*writeln ("<p>" + getLocalization(txt_results_for) + " " + "<span class=\"searchExpression\">"  + cleanwordsList + "</span>" + "<br/>"+"</p>");*/
        results = "<p>";
        //write("<ul class='searchresult'>");
        for (t in linkTab) {
            results += linkTab[t].toString();
        }
        results += "</p>";
    } else {
        results = "<p>" + getLocalization("Search no results") + " " + "<span class=\"searchExpression\">" + txt_wordsnotfound + "</span>" + "</p>";
    }
    
    //-------------------------OXYGEN PATCH START-------------------------
    // Verify if the browser is Google Chrome and the WebHelp is used on a local machine
    // If browser is Google Chrome and WebHelp is used on a local machine a warning message will appear
    // Highlighting will not work in this conditions. There is 2 workarounds
    if (verifyBrowser()){
        document.getElementById('searchResults').innerHTML = results;
    } else {
        document.getElementById('searchResults').innerHTML = warningMsg + results;
    }
    //-------------------------OXYGEN PATCH END-------------------------
}

//-------------------------OXYGEN PATCH START-------------------------
// Verify if the stemmed word is aproximately the same as the searched word
function verifyWord(word, arr){
  for (var i = 0 ; i < arr.length ; i++){
    if (word[0] == arr[i][0] 
      && word[1] == arr[i][1] 
      //&& word[2] == arr[i][2]
      ){
      return true;
    }
  }
  return false;
}

// Look for elements that start with searchedValue.
function wordsStartsWith(searchedValue){
  var toReturn = '';
  for (var sv in w){
    if (searchedValue.length < 3){
      continue;
    } else {
      if (sv.toLowerCase().indexOf(searchedValue.toLowerCase()) == 0){
        toReturn+=sv + ","; 
      }
    }
  }
  return toReturn.length > 0 ? toReturn : undefined;
}
//-------------------------OXYGEN PATCH END-------------------------

function tokenize(wordsList){
    var stemmedWordsList = new Array(); // Array with the words to look for after removing spaces
    var cleanwordsList = new Array(); // Array with the words to look for
    for(var j in wordsList){
        var word = wordsList[j];
        if(typeof stemmer != "undefined" ){
            stemQueryMap[stemmer(word)] = word;
        } else {
            stemQueryMap[word] = word;
        }
    } 
     //stemmedWordsList is the stemmed list of words separated by spaces.
    for (var t in wordsList) {
        wordsList[t] = wordsList[t].replace(/(%22)|^-/g, "");
        if (wordsList[t] != "%20") {
            scriptLetterTab.add(wordsList[t].charAt(0));
            cleanwordsList.push(wordsList[t]);
        }
    }

    if(typeof stemmer != "undefined" ){
        //Do the stemming using Porter's stemming algorithm
        for (var i = 0; i < cleanwordsList.length; i++) {     
            var stemWord = stemmer(cleanwordsList[i]);      
            stemmedWordsList.push(stemWord);
        }
    } else {
        stemmedWordsList = cleanwordsList;
    }
    return stemmedWordsList;
}

//Invoker of CJKTokenizer class methods.
function cjkTokenize(wordsList){
    var allTokens= new Array();
    var notCJKTokens= new Array();
    var j=0;
    for(j=0;j<wordsList.length;j++){
        var word = wordsList[j];
        if(getAvgAsciiValue(word) < 127){
            notCJKTokens.push(word);
        } else { 
            var tokenizer = new CJKTokenizer(word);
            var tokensTmp = tokenizer.getAllTokens();
            allTokens = allTokens.concat(tokensTmp);
        }
    }
    allTokens = allTokens.concat(tokenize(notCJKTokens));
    return allTokens;
}

//A simple way to determine whether the query is in english or not.
function getAvgAsciiValue(word){
    var tmp = 0;
    var num = word.length < 5 ? word.length:5;
    for(var i=0;i<num;i++){
        if(i==5) break;
        tmp += word.charCodeAt(i);
    }
    return tmp/num;
}

//CJKTokenizer
function CJKTokenizer(input){
    this.input = input;
    this.offset=-1;
    this.tokens = new Array(); 
    this.incrementToken = incrementToken;
    this.tokenize = tokenize;
    this.getAllTokens = getAllTokens;
    this.unique = unique;

    function incrementToken(){
    if(this.input.length - 2 <= this.offset){
      return false;
    }
    else {
      this.offset+=1;
      return true;
    }
  }

  function tokenize(){
    //document.getElementById("content").innerHTML += x.substring(offset,offset+2)+"<br>";
    return this.input.substring(this.offset,this.offset+2);
  }

  function getAllTokens(){
    while(this.incrementToken()){
      var tmp = this.tokenize();
      this.tokens.push(tmp);
    }
        return this.unique(this.tokens);
//    document.getElementById("content").innerHTML += tokens+" ";
//    document.getElementById("content").innerHTML += "<br>dada"+sortedTokens+" ";
    /*for(i=0;i<tokens.length;i++){
      var ss = tokens[i] == sortedTokens[i];

//      document.getElementById("content").innerHTML += "<br>dada"+un[i]+"- "+stems[i]+"&nbsp;&nbsp;&nbsp;"+ ss;
      document.getElementById("content").innerHTML += "<br>"+sortedTokens[i];
    }*/
  }

  function unique(a)
  {
     var r = new Array();
     o:for(var i = 0, n = a.length; i < n; i++)
     {
        for(var x = 0, y = r.length; x < y; x++)
        {
     if(r[x]==a[i]) continue o;
        }
        r[r.length] = a[i];
     }
     return r;
  } 
}


/* Scriptfirstchar: to gather the first letter of index js files to upload */
function Scriptfirstchar() {
    this.strLetters = "";
    this.add = addLettre;
}

function addLettre(caract) {

    if (this.strLetters == 'undefined') {
        this.strLetters = caract;
    } else if (this.strLetters.indexOf(caract) < 0) {
        this.strLetters += caract;
    }

    return 0;
}
/* end of scriptfirstchar */

/*main loader function*/
/*tab contains the first letters of each word looked for*/
function loadTheIndexScripts(tab) {

    //alert (tab.strLetters);
    var scriptsarray = new Array();

    for (var i = 0; i < tab.strLetters.length; i++) {

        scriptsarray[i] = "..\/search" + "\/" + tab.strLetters.charAt(i) + ".js";
    }
    // add the list of html files
    i++;
    scriptsarray[i] = "..\/search" + "\/" + htmlfileList;

    //debug
    for (var t in scriptsarray) {
        //alert (scriptsarray[t]);
    }

    tab = new ScriptLoader();
    for (t in scriptsarray) {
        tab.add(scriptsarray[t]);
    }
    tab.load();
    //alert ("scripts loaded");
    return (scriptsarray);
}

/* ScriptLoader: to load the scripts and wait that it's finished */
function ScriptLoader() {
    this.cpt = 0;
    this.scriptTab = new Array();
    this.add = addAScriptInTheList;
    this.load = loadTheScripts;
    this.onScriptLoaded = onScriptLoadedFunc;
}

function addAScriptInTheList(scriptPath) {
    this.scriptTab.push(scriptPath);
}

function loadTheScripts() {
    var script;
    var head;

    head = document.getElementsByTagName('head').item(0);

    //script = document.createElement('script');

    for (var el in this.scriptTab) {
        //alert (el+this.scriptTab[el]);
        script = document.createElement('script');
        script.src = this.scriptTab[el];
        script.type = 'text/javascript';
        script.defer = false;

        head.appendChild(script);
    }

}

function onScriptLoadedFunc(e) {
    e = e || window.event;
    var target = e.target || e.srcElement;
    var isComplete = true;
    if (typeof target.readyState != undefined) {

        isComplete = (target.readyState == "complete" || target.readyState == "loaded");
    }
    if (isComplete) {
        ScriptLoader.cpt++;
        if (ScriptLoader.cpt == ScriptLoader.scripts.length) {
            ScriptLoader.onLoadComplete();
        }
    }
}

/*
function onLoadComplete() {
    alert("loaded !!");
} */

/* End of scriptloader functions */
 
// Array.unique( strict ) - Remove duplicate values
function unique(tab) {
    var a = new Array();
    var i;
    var l = tab.length;

    if (tab[0] != undefined) {
        a[0] = tab[0];
    }
    else {
        return -1;
    }

    for (i = 1; i < l; i++) {
        if (indexof(a, tab[i], 0) < 0) {
            a.push(tab[i]);
        }
    }
    return a;
}
function indexof(tab, element, begin) {
    for (var i = begin; i < tab.length; i++) {
        if (tab[i] == element) {
            return i;
        }
    }
    return -1;

}
/* end of Array functions */


/*
 Param: mots= list of words to look for.
 This function creates an hashtable:
 - The key is the index of a html file which contains a word to look for.
 - The value is the list of all words contained in the html file.

 Return value: the hashtable fileAndWordList
 */
function SortResults(mots) {

    var fileAndWordList = new Object();
    if (mots.length == 0 || mots[0].length == 0) {
        return null;
    }
    
    //-------------------------OXYGEN PATCH START-------------------------
    // In generated js file we add scoring at the end of the word
    // Example word1*scoringForWord1,word2*scoringForWord2 and so on
    // Split after * to obtain the right values
    var scoringArr = Array();
    for (var t in mots) {
        // get the list of the indices of the files.
        var listNumerosDesFicStr = w[mots[t].toString()];        
        //alert ("listNumerosDesFicStr "+listNumerosDesFicStr);
        var tab = listNumerosDesFicStr.split(",");
        //for each file (file's index):
        for (var t2 in tab) {
            var tmp = '';
            var idx = '';
            var temp = tab[t2].toString();
            if (temp.indexOf('*') != -1){
                idx = temp.indexOf('*');
                tmp = temp.substring(idx + 3, temp.length);
                temp = temp.substring(0,idx);
            }
            scoringArr.push(tmp);
            if (fileAndWordList[temp] == undefined) {
                fileAndWordList[temp] = "" + mots[t];
            } else {
                fileAndWordList[temp] += "," + mots[t];
            }
            //console.info("fileAndWordList[" + temp + "]=" + fileAndWordList[temp] + " : " + tmp);
        }
    }
    var fileAndWordListValuesOnly = new Array();
    // sort results according to values
    var temptab = new Array();
    finalObj = new Array();
    for (t in fileAndWordList) {      
      finalObj.push(new newObj(t,fileAndWordList[t]));
    }    
    finalObj = removeDerivates(finalObj);
    for (t in finalObj) {
        tab = finalObj[t].wordList.split(',');
        var tempDisplay = new Array();
        for (var x in tab) {            
            if(stemQueryMap[tab[x]] != undefined && doStem){
                tempDisplay.push(stemQueryMap[tab[x]]); //get the original word from the stem word.                
            } else {
                tempDisplay.push(tab[x]); //no stem is available. (probably a CJK language)
            }
        }
        var tempDispString = tempDisplay.join(", ");
        var index;
        for (x in fileAndWordList) {
          if (x === finalObj[t].filesNo) {
            index = x;
            break;
          }
        }
        var scoring = findRating(fileAndWordList[index], index);  
        temptab.push(new resultPerFile(finalObj[t].filesNo, finalObj[t].wordList, tab.length, tempDispString, scoring));
        fileAndWordListValuesOnly.push(finalObj[t].wordList);        
    }
    fileAndWordListValuesOnly = unique(fileAndWordListValuesOnly);
    fileAndWordListValuesOnly = fileAndWordListValuesOnly.sort(compare_nbMots);

    var listToOutput = new Array();
    for (var j in fileAndWordListValuesOnly) {
        for (t in temptab) {
            if (temptab[t].motsliste == fileAndWordListValuesOnly[j]) {
                if (listToOutput[j] == undefined) {
                    listToOutput[j] = new Array(temptab[t]);
                } else {
                    listToOutput[j].push(temptab[t]);
                }
            }
        }
    }   
  // Sort results by scoring, descending on the same group
  for (var i in listToOutput) {
      listToOutput[i].sort(function(a, b){
      return b.scoring - a.scoring;
    });
  }
  // If we have groups with same number of words, 
  // will sort groups by higher scoring of each group
  for (var i = 0; i < listToOutput.length - 1; i++) {
    for (var j = i + 1; j < listToOutput.length; j++) {
      if (listToOutput[i][0].motsnb < listToOutput[j][0].motsnb 
        || (listToOutput[i][0].motsnb == listToOutput[j][0].motsnb
        && listToOutput[i][0].scoring < listToOutput[j][0].scoring)
        ) {
        var x = listToOutput[i];
        listToOutput[i] = listToOutput[j];
        listToOutput[j] = x;
      }
    }
  }

    return listToOutput;
}

// Remove derivates words from the list of words
function removeDerivates(obj){
  var toResultObject = new Array(); 
  for (i in obj){
    var filesNo  = obj[i].filesNo;
    var wordList = obj[i].wordList;
    var wList = wordList.split(",");    
    var searchedWords = searchTextField.toLowerCase().split(" ");
    for (var k = 0 ; k < searchedWords.length ; k++){
      for (var j = 0 ; j < wList.length ; j++){       
        if (wList[j].startsWith(searchedWords[k])){
          wList[j] = searchedWords[k];
        }
      }
    }
    wList = removeDuplicate(wList);
    var recreateList = '';
    for(var x in wList){
      recreateList+=wList[x] + ",";
    }
    recreateList = recreateList.substr(0, recreateList.length - 1);
    toResultObject.push(new newObj(filesNo, recreateList));
  }
  return toResultObject;
}

function newObj(filesNo, wordList){
  this.filesNo = filesNo;
  this.wordList = wordList;
}
//-------------------------OXYGEN PATCH END-------------------------


// Object.
// Oxygen. Add a new parameter. Scoring.
function resultPerFile(filenb, motsliste, motsnb, motslisteDisplay, scoring, group) {
  //10 - spring,time - 2 - spring, time - 55 - 3
    this.filenb = filenb;
    this.motsliste = motsliste;
    this.motsnb = motsnb;
    this.motslisteDisplay= motslisteDisplay;
    //-------------------------OXYGEN PATCH START-------------------------
    this.scoring = scoring;
    //-------------------------OXYGEN PATCH END-------------------------
}

//-------------------------OXYGEN PATCH START-------------------------
function findRating(words, nr){
    var sum = 0;
    var xx = words.split(',');
    for (jj = 0 ; jj < xx.length ; jj++){
        var wrd = w[xx[jj]].split(',');
        for (var ii = 0 ; ii < wrd.length ; ii++){
            var wrdno = wrd[ii].split('*');
            if (wrdno[0] == nr){
                sum+=parseInt(wrdno[1]);
            }
        }
    }
    return sum;
}
//-------------------------OXYGEN PATCH END-------------------------
function compare_nbMots(s1, s2) {
    var t1 = s1.split(',');
    var t2 = s2.split(',');
    //alert ("s1:"+t1.length + " " +t2.length)
    if (t1.length == t2.length) {
        return 0;
    } else if (t1.length > t2.length) {
        return 1;
    } else {
        return -1;
    }
    //return t1.length - t2.length);
}
//-------------------------OXYGEN PATCH START-------------------------
// return false if browser is Google Chrome and WebHelp is used on a local machine, not a web server 
function verifyBrowser(){
    var returnedValue = true;    
    var browser = BrowserDetect.browser;
    var addressBar = window.location.href;
    if (browser == 'Chrome' && addressBar.indexOf('file://') === 0){
        returnedValue = false;
    }
    
    return returnedValue;
}

// Remove duplicate values from an array
function removeDuplicate(arr) {
   var r = new Array();
   o:for(var i = 0, n = arr.length; i < n; i++) {
      for(var x = 0, y = r.length; x < y; x++) {
         if(r[x]==arr[i]) continue o;
      }
      r[r.length] = arr[i];
   }
   return r;
}

// Create startsWith method
String.prototype.startsWith = function(str) {
  return (this.match("^"+str)==str);
}

function trim(str, chars) {
  return ltrim(rtrim(str, chars), chars);
}
 
function ltrim(str, chars) {
  chars = chars || "\\s";
  return str.replace(new RegExp("^[" + chars + "]+", "g"), "");
}
 
function rtrim(str, chars) {
  chars = chars || "\\s";
  return str.replace(new RegExp("[" + chars + "]+$", "g"), "");
}

//-------------------------OXYGEN PATCH END-------------------------