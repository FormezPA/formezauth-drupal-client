// formezauth.js

if(window.opener!=undefined){ 
	window.opener.location=window.parent.location; 
	window.self.close();
}