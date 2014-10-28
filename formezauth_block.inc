<?php


/**
 * ---------------------------------------
 *	BLOCCO FORMEZ AUTH per la connessione
 * ---------------------------------------
 */


/**
 * Implements hook_block_info().
 */
function formezauth_block_info() {
  $blocks = array();
  $blocks['formezauth_login'] = array(
    'info' => t('FormezAuth Login'),
    'cache' => DRUPAL_NO_CACHE,
  );
 
  return $blocks;
}

/**
 * Implements hook_block_view().
 */
function formezauth_block_view($delta='') {
  $block = array();
 
  switch($delta) {
    case 'formezauth_login' :
      $block['content'] = _formezauth_block_login();
      break;
  }
 
  return $block;
}

/**
 * Contenuto del blocco FormezAuth Login
 * Crea anche un punto di alter per permettere
 * personalizzazioni su 
 * hook_formezauth_block_login_alter
 */
function _formezauth_block_login(){

	global $base_url;

	$label_submit=variable_get('formezauth_block_submit_label', 'Accedi con FormezAuth');

	$out="<div class=\"formezauth-block-login\">";
  if(variable_get('formezauth_open_in_popup', TRUE) == TRUE){

    $js_button="window.open('".$base_url."/connect/oauthconnector_formezauth','','scrollbars=yes,resizable=yes,width=800,height=800', 'formezauth');";

    $out.="<input type=\"button\" value=\"".$label_submit."\" class=\"form-submit\" onclick=\"" . $js_button . "\" />\n";
  }
  else{
  	$out.="<form method=\"post\" action=\"".$base_url."/connect/oauthconnector_formezauth\">\n";
  	$out.="<input type=\"submit\" value=\"".$label_submit."\" class=\"form-submit\" />\n";
  	$out.="</form>\n";
  }
	$out.="</div>\n";

	$vars=array($base_url, $label_submit);

	drupal_alter('formezauth_block_login', $out, $vars);

	return $out;
}
