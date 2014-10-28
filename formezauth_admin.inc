<?php


function _formezauth_variabili_condivise(){

	$form['formezauth_account_create'] = array(
    '#type' => 'checkbox',
    '#title' => t('Permetti la creazione di account da FormezAuth'),
    '#default_value' => variable_get('formezauth_account_create', FALSE),
    '#description' => t('FormezAuth creeà automaticamente nuovi user accounts dopo avvenuta autenticazione a prescindere dai settaggi locali di creazione degli user accounts.'),
  );

/*
  $form['formezauth_disable_profile_change'] = array(
    '#type' => 'checkbox',
    '#title' => t('Disabilita la modifica locale dei campi del profilo'),
    '#default_value' => variable_get('formezauth_disable_profile_change', FALSE),
    '#description' => t('Se spuntata, questa opzione disabiliterà le modifiche del profilo da parte dell\'utente, che potrà modificare il suo profilo solo dal server FormezAuth.'),
  );
*/


	$options_reg = array(
	  1 => t('Disabilita la modifica locale dei campi sincronizzati'), 
	  2 => t('Disabilita la modifica locale di tutto il profilo'),
		0 => t('Permetti la modifica dei campi in locale (saranno comunque sovrascritti al login)'),
	);

  $form['registration_cancellation']['formezauth_disable_profile_change'] = array(
    '#type' => 'radios',
    '#title' => t('Modifica locale del profilo'),
    '#options' => $options_reg,
    '#default_value' => variable_get('formezauth_disable_profile_change', 1),
    '#description' => t('Definisce come gestire le modifiche del profilo da parte dell\'utente, che potrà modificare il suo profilo solo dal server FormezAuth.'),
  );

  return $form;
}

/**
 * Callback di admin/config/services/formezauth/refresh_fields
 * Popola la tabella formezauth_server_map che a sua volta popola le 
 * tendine con i campi disponibili inviati dal server.
 */
function _formezauth_server_fields(){

	// mostra il form
	$out='';
	$out.=drupal_render(drupal_get_form('_formezauth_refresh_fields'));

	// interroga la tabella
	$results=db_query("SELECT * FROM {formezauth_server_map} ");

  $header = array(t('Campo'), t('Nome'), t('Descrizione'), t('Tipo'));
	$rows = array();

  // Stampa la documentazione dei campi
  foreach($results as $row){

  	$desc=json_decode($row->info);

  	if(is_object($desc)){

	  	$rows[]=array(
	  		$desc->label,
	  		$row->fa_field,
	  		$desc->description,
	  		$desc->widget->module,
	  	);
	  }
	  else{

	  	$rows[]=array('',$row->fa_field,'','');
	  }
  }

  $out.="<h2>Campi disponibili sul server</h2>\n";
  $out.=theme('table', array('header' => $header, 'rows' => $rows));

  return $out;
}


/**
 *  Form per l'aggiornamento dei campi
 */
function _formezauth_refresh_fields($form, &$form_state) {
  // Submit

	$form['#action'] = url('admin/config/services/formezauth/refresh_fields');

	//$form_state['redirect']=url();

	$form['refresh'] = array(
	  '#type' => 'fieldset', 
	  '#title' => t('Sincronizzazione dei campi dal server'), 
	  '#weight' => 5, 
	  '#collapsible' => TRUE, 
	  '#collapsed' => FALSE,
	);

  $form['refresh']['submit'] = array(
    '#type' => 'submit',
    '#value' => 'Sincronizza ora',
  );
  
  return $form;
}


/**
 * Callback di admin/config/services/formezauth/refresh_fields
 * Popola la tabella formezauth_server_map che a sua volta popola le 
 * tendine con i campi disponibili inviati dal server.
 *
 * @TODO: mostrare il numero di colonne cambiate in caso di sincronizzazione
 */
function _formezauth_refresh_server_fields($redirect=true){

  $url_server=variable_get('formezauth_server_addr');
  $url_server.= (substr($url_server,-1,1)=='/') ? "doc/fields" : "/doc/fields";

  if(function_exists('curl_init')){
    $ch = curl_init($url_server);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
    curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, 0);
    curl_setopt ($ch, CURLOPT_SSL_VERIFYHOST, 0);
    $file_content = (array) curl_exec($ch);
    curl_close($ch);
  }
  else{

    $file_content=@file($url_server);
  }

  if(!is_array($file_content)){

  	drupal_set_message(t('Errore nella configurazione del Server FormezAuth. Impossibile trovare la documentazione dei campi su @url', array('@url'=>$url_server)), 'error');
  }
  else{

  	$campi_server=json_decode(join('',$file_content));

		if(isset($campi_server->user) && is_object($campi_server->user)){

			foreach($campi_server->user as $nome=>$val){

				$nomi[]=$nome;
			}


	  	$q1=db_query("DELETE FROM {formezauth_server_map} WHERE fa_field NOT IN ('".implode("','",$nomi)."')");

	  	$q2=db_query("DELETE FROM {formezauth_fmap} WHERE fa_field NOT IN ('uid', 'mail','".implode("','",$nomi)."')");

	  	$affected_2=0;


	  	foreach($campi_server->user as $f=>$cc){

	  		//$q2[]=db_query("INSERT IGNORE INTO {formezauth_fmap} (fa_field) VALUES ('".$f."')");

				$affected_2+= db_merge('formezauth_server_map')
					  ->key(array(
					      'fa_field' => $f,

					  ))
					  ->fields(array(
					    'fa_field' => $f,
					    'info' => json_encode($cc)
					  ))->execute();
	  	}

	  	drupal_set_message(t('Campi aggiornati correttamente'));
	  }
	  else{
	  	drupal_set_message(t('Errore nella configurazione del Server FormezAuth. Impossibile eseguire il parsing su @url', array('@url'=>$url_server)), 'error');
	  }
	}

	if($redirect){
		drupal_goto('admin/config/services/formezauth/fields');
	}
}


/**
 * Pagina di istruzioni per l'amministratore
 */
function _formezauth_hello(){

	global $base_url;

	// Atteso: 1
	$user_register=variable_get('user_register');
	$skip_verification=variable_get('formezauth_account_create');


	if($user_register!=1 && $skip_verification==0){

		drupal_set_message('La configurazione degli utenti non è impostata in modo coerente per FormezAuth. Leggi sotto per i dettagli','warning');
	}

	if(!$skip_verification){
		$stato_user='<strong>DISABILITATO</strong>.<br />';
		$stato_user.="Vai sulla <a href=\"".$base_url."/admin/config/people/accounts\">configurazione degli utenti</a> 
		per modificare la configurazione.";
	}
	else{
		$stato_user='<strong>ABILITATO</strong>.';
	}

	$out="<h2>Configurazione modulo FormezAuth</h3>\n";

	$out.="<p>FormezAuth è un modulo che permette l'autenticazione tramite OAuth del server Formez<br />
	Per utilizzare il modulo sono necessari i seguenti passaggi:</p>
	
	<h3>1. Richiedere una <em>OAuth Consumer Key</em> e <em>OAuth Consumer Secret</em></h3>
	<p>Queste devono essere richieste all'amministratore del sistema FormezAuth. 
	Scrivi a <a href=\"mailto:webmaster@formez.it\">webmaster@formez.it</a> per fartene fornire una.<br />
	Una volta ottenute le Consumer Key e Consumer Secret devi inserirle negli omonimi campi del
	<a href=\"".$base_url."/admin/structure/oauthconnector/formezauth/edit\">connettore FormezAuth</a>.</p>

	<h3>2. Configurazione degli utenti e dei permessi</h3>
	<p>E' necessario abilitare il permesso <em>Connect with formezauth</em> per l'<em>anonymous</em>.
	<p>FormezAuth può autenticare gli utenti anche se attualmente le politiche del sito
	non permettono la creazione diretta di nuovi utenti.<br />
	Per fare questo è necessario impostare il checkbox nella configurazione utente,
	sulla variabile <strong><em>permetti la creazione di account da FormezAuth</em></strong>. <br />
	Attualmente questo è ".$stato_user.".</p>

	<h3>3. Mappatura dei campi</h3>
	<p>Di default la connessione di FormezAuth trasferisce il <em>nickname</em> (<em>Drupal name</em>) e l'<em>email</em>.<br />
	Sono disponibili molti altri campi: se si vogliono trasferire su questo sito anche altri campi, è necessario
	eseguire la <a href=\"".$base_url."/admin/config/services/formezauth/map\">mappatura dei campi disponibili</a>.
	I campi mappati saranno aggiornati <strong>ad ogni accesso dell'utente</strong>.</p>

	<h3>4. Creazione del blocco FormezAuth Login</h3>
	<p>Il modulo FormezAuth mette a disposizione un blocco per il login, di default disabilitato.
	Vai nel <a href=\"".$base_url."/admin/structure/block\">menu Blocchi</a> per abilitarlo 
	(ad esempio nella <em>sidebar first</em>).
	ed imposta la <strong>visibilità</strong> solo per <strong>gli utenti anonimi</strong>.<br />
	L'etichetta del pulsante è personalizzabile dalle <a href=\"".$base_url."/admin/config/services/formezauth/init\">variabili FormezAuth</a>.
	Il blocco è visibile solo se il permesso <em>Connect with formezauth</em> è abilitato per gli anonimi.</p>

	<h3>Delegare il controllo degli utenti a FormezAuth</h3>
	<p>E' possibile disabilitare la modifica degli utenti su questo sito,
	facendo sì che possano operare le modifiche sull'account solo su FormezAuth.
	<br />
	Per farlo è necessario abilitare l'opzione <strong>Disabilita la modifica locale dei campi del profilo</strong>.<br />
	Attenzione: in questo caso non sarà possibile operare sul profilo da parte degli utenti</p>.
	";

	return $out;
}


/**
 * Implements hook_help().
 */
function formezauth_help($path, $arg) {

  $output = '';  //declare your output variable
  switch ($path) {
    case "admin/help#formezauth":
      $output = '<p>'.  t("Modulo di autenticazione tramite FormezAuth") .'</p>';
      break;
  }
  return $output;
} 



/**
 * Page callback
 *
 * @see formezauth_menu()
 */
function formezauth_form($form, &$form_state) {

  $form=array();

  $form['formezauth_server_addr'] = array(
    '#type' => 'textfield',
    '#title' => t('URL del server FormezAuth'),
    '#default_value' => variable_get('formezauth_server_addr'),
    '#size' => 60,
    '#maxlength' => 255,
    '#description' => t('Indirizzo del server FormezAuth.'),
    '#required' => TRUE,
  );
  $form['formezauth_block_submit_label'] = array(
    '#type' => 'textfield',
    '#title' => t('Etichetta del pulsante di accesso'),
    '#default_value' => variable_get('formezauth_block_submit_label', 'Accedi con FormezAuth'),
    '#size' => 60,
    '#maxlength' => 255,
    '#description' => t('Etichetta del pulsante del blocco FormezAuth.'),
    '#required' => FALSE,
  );
  $form['formezauth_debug_log_access'] = array(
    '#type' => 'checkbox',
    '#title' => t('Log di debug per l\'accesso dell\'utente'),
    '#default_value' => variable_get('formezauth_debug_log_access', FALSE),
    '#description' => t('Mostra il debug del log di accesso su watchdog.'),
  );
  $form['formezauth_open_in_popup'] = array(
    '#type' => 'checkbox',
    '#title' => t('Apri FormezAuth in un popup'),
    '#default_value' => variable_get('formezauth_open_in_popup', TRUE),
    '#description' => t('Se selezionato, il tasto di connessione a FormezAuth aprirà una finestrella (stile Twitter), in caso contrario il login avverrà nella finestra principale.'),
  );

  $json_image_styles=variable_get('formezauth_server_addr')."/doc/image_styles";


  if(function_exists('curl_init')){
    $ch = curl_init($json_image_styles);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
    curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, 0);
    curl_setopt ($ch, CURLOPT_SSL_VERIFYHOST, 0);
    $str_json_is = curl_exec($ch);
    curl_close($ch);
  }
  else{
    $str_json_is=file_get_contents($json_image_styles);
  }


  $obj=json_decode($str_json_is);

  $options['']=t('- Non importare le immagini -');

  if(is_object($obj)){
  	foreach($obj as $k=>$o){
  		$options[$o->path]=$k;
  	}
  }

  $form['formezauth_manage_image'] = array(
    '#type' => 'select',
    '#options' => $options,
    '#title' => t('Modello per l\'importazione delle immagini utente'),
    '#default_value' => variable_get('formezauth_manage_image', ''),
    '#description' => t('Permette di scegliere il modello di immagine per dimensione.'),
  );


  $form+=_formezauth_variabili_condivise();

  // quando vengono aggiornate le variabili, devono essere aggiornati anche il connector
  if(count($form_state['input'])>0 && isset($form_state['input']['formezauth_server_addr'])){

  	// _formezauth_sync_connector_new_url($form_state['input']['formezauth_server_addr']);
  }

	return system_settings_form($form);
}


/**
 * Page callback
 * Chiamato dalla pagina admin/config/services/formezauth/map
 *
 */
function formezauth_form2($form, &$form_state) {

  $form=array();

  $HTML_INFO="
  <h2>Mappatura dei campi locali</h2>
  <p>Da qui è possibile mappare i propri campi (definiti nel profilo utente)
  con quelli trasmessi da FormezAuth.<br />
  I campi verranno aggiornati dal server al client (questa installazione Drupal) 
  ad ogni login dell'utente.<br />
  Nelle tendine sono mostrati i campi inviati dal server.</p>
  ";

  $form['info_h2'] = array(
		  '#markup' => $HTML_INFO
		);

  // Valori possibili: campi locali
//  $ffa=field_info_instances('user',NULL);
  $ffa=field_info_instances();

  $ff=array();
  foreach($ffa as $k=>$v){

  	if($k=='user'){
  		$ff['user']= (array) array_keys($v['user']);
  	}
  	else if(module_exists('profile2') && $k=='profile2'){

  		foreach($v as $kk=>$vv)
  		$ff[$kk]=(array) array_keys($vv);	
  	}
  }

  $campi_locali=$ff;

  if(is_array($ff) && count($ff)>0){
	  foreach($ff as $f){

	  	//$campi_locali[$f]=$f;
	  }
	}
	else{
		
		drupal_set_message(t('Nessun campo per il profilo di questo sito. <br />'
		  						.'Aggiungi dei campi al profilo per mappare i campi del server.'), 'warning');
	}





	$result = db_query("SELECT fa_field FROM {formezauth_server_map} ORDER BY fa_field");

	// prepara l'array con i default
	$fa_fields=array(
		''=>'', 
		'uid'=>'uid',
		'mail'=>'mail',
	);

	foreach ($result as $row) {
		$fa_fields[$row->fa_field]=$row->fa_field;
	}


	$result = db_query("SELECT local_field, fa_field, entity_type FROM {formezauth_fmap}");
	foreach ($result as $row) {
		$attuali[$row->local_field]= $row;
	}

  foreach($campi_locali as $category=>$campi){

  	$form['__'.$category]=array(
  			'#markup'=>'<h3>'.$category."</h3>\n",
  		); 

		foreach($campi as $label=>$local_k){

      $info_field=field_info_field($local_k);

      if(isset($info_field['columns']) && count($info_field['columns'])>1 ){

        foreach($info_field['columns'] as $col=>$colvalue){

          $default_v = (isset($attuali[$local_k.":".$col]->fa_field)) ? $attuali[$local_k.":".$col]->fa_field : '';

          // Perform operations on $record->title, etc. here.
          $form['ff['.$category.":".$local_k.':'.$col.']'] = array(
             '#type' => 'select',
             '#title' => " Mappatura ".$local_k.":".$col,
             '#options' => $fa_fields, 
             '#default_value' => $default_v,
             '#description' => t('Mappatura del campo locale :campo con i campi di FormezAuth.', array(":campo"=>$local_k.":".$col)),
            );
        }
      }
      else{

  	  	$default_v = (isset($attuali[$local_k]->fa_field)) ? $attuali[$local_k]->fa_field : '';


  	  	// Perform operations on $record->title, etc. here.
  		  $form['ff['.$category.":".$local_k.']'] = array(
  	       '#type' => 'select',
  	       '#title' => " Mappatura ".$local_k,
  	       '#options' => $fa_fields, 
  	       '#default_value' => $default_v,
  	       '#description' => t('Mappatura del campo locale :campo con i campi di FormezAuth.', array(":campo"=>$local_k)),
  				);
      }
	  }
	}

	$form['submit'] = array(
	    '#type' => 'submit',
	    '#value' => t('Save'),
	);

  //return system_settings_form($form);
	return $form;
}


function formezauth_form2_submit($form, &$form_data){

	$num_updated=0;
	$num_del=0;

	if(isset($form_data['input']['ff'])){

		// azzera i valori
		$num_del=db_query("DELETE FROM {formezauth_fmap}");

		foreach($form_data['input']['ff'] as $ff=>$fv){

			list($entity_type, $local_field)=explode(":", $ff,2);

			$num_updated+= db_insert('formezauth_fmap')
			  ->fields(array(
			    'local_field' => $local_field,
			    'fa_field' => $fv,
			    'entity_type' => $entity_type,
			  ))
			  ->execute();
		}
	}

	//drupal_set_message(t('Mappatura campi aggiornata: @num campi impostati', array('@num'=>$num_updated)));
	drupal_set_message(t('Mappatura campi aggiornata: campi impostati'));
}





/**
 * Implements validation from the Form API.
 * 
 * @param $form
 *   A structured array containing the elements and properties of the form.
 * @param $form_state
 *   An array that stores information about the form's current state 
 *   during processing.
 */
function formezauth_form_validate($form, &$form_state){

  $url_server = $form_state['values']['formezauth_server_addr'];
  if (!filter_var($url_server, FILTER_VALIDATE_URL)){
    form_set_error('formezauth_server_addr', t('L\'indirizzo del server deve essere una URL corretta, compreso http:// o https://'));
  }
  else if(substr($url_server, -1, 1)=='/'){
  	form_set_error('formezauth_server_addr', t('L\'indirizzo del server non deve avere lo slash finale'));
  }

}



/**
*  Implements hook__form_user_admin_settings_alter
* 
*    Crea un checkbox in Configurazione -> Account settings
*    admin/config/people/accounts
*  	 DA SPUNTARE per far funzionare.
*    Inoltre è necessario disabilitare la conferma dell'email dalla stessa pagina
**/ 
function formezauth_form_user_admin_settings_alter(&$form) {


  $form['registration_cancellation']['formezauth_account_create'] = array(
    '#type' => 'checkbox',
    '#title' => t('Permetti la creazione di account da FormezAuth'),
    '#default_value' => variable_get('formezauth_account_create', FALSE),
    '#description' => t('FormezAuth creeà automaticamente nuovi user accounts dopo avvenuta autenticazione a prescindere dai settaggi locali di creazione degli user accounts.'),
  );

  $form_add['registration_cancellation']=_formezauth_variabili_condivise();

  $form+=$form_add;
}



/**
 * Implements hook_default_oauthconnector_provider().
 */
function formezauth_default_oauthconnector_provider() {

	$providers = array();

	$provider = new stdClass();
	$provider->disabled = TRUE; /* Edit this to true to make a default provider disabled initially */
	$provider->name = 'formezauth';
	$provider->title = 'formezauth';
	$provider->url = variable_get('formezauth_server_addr');
	$provider->consumer_advanced = array(
	  'oauth2' => 0,
	  'signature method' => 'HMAC-SHA1',
	  'authentication realm' => '',
	  'request token endpoint' => '/oauth/request_token',
	  'authorization scope' => '',
	  'authorization endpoint' => '/oauth/authorize',
	  'access token endpoint' => '/oauth/access_token',
	);
	$provider->mapping = array(
	  'fields' => array(
	  	'uid' => array(
	      'resource' => $provider->url.'/oauthlogin/api/oauthlogin/info',
	      'method post' => 1,
	      'field' => 'uid',
	      'querypath' => FALSE,
	      'sync_with_field' => '',
	    ),
	    'mail' => array(
	      'resource' => $provider->url.'/oauthlogin/api/oauthlogin/info',
	      'method post' => 1,
	      'field' => 'mail',
	      'querypath' => FALSE,
	      'sync_with_field' => 'mail',
	    ),
	    'name' => array(
	      'resource' => $provider->url.'/oauthlogin/api/oauthlogin/info',
	      'method post' => 1,
	      'field' => 'name',
	      'querypath' => FALSE,
	      'sync_with_field' => 'name',
	    ),
	  ),
	  'format' => 'json',
	);

	$providers['formezauth'] = $provider;

  return $providers;
}
