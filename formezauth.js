// formezauth.js

if(window.opener!=undefined && window.location.search =='?fa=1'){ 
	window.opener.location=window.parent.location; 
	window.self.close();
}
