<?php

/**
 * 	Implements hook_user_login
 *  Funzione di mappatura dei campi da sovrascrivere ad ogni login (sincronizzazione)
 *
 **/
function formezauth_user_login(&$edit, $account) {

  global $user, $base_url;

  if (strpos($account->init, 'oauthconnector_formezauth') === false){
    drupal_set_message(t('An error occurred and processing did not complete. [SESSION:OAUTH_CONNECTOR_DOESNT_EXISTS]'), 'error');
    return;
  }

  drupal_alter('formezauth_user_login_pre', $edit, $account);

  if (!isset($_SESSION['oauth_user_data']) || !is_array($_SESSION['oauth_user_data'])){
    drupal_set_message(t('An error occurred and processing did not complete. [SESSION:OAUTH_USER_DATA_NOT_EXISTS]'), 'error');
    return ;
  }

  // Settings
  // interroga la tabella per determinare i tipi
  $settings_info = db_query("SELECT * FROM {formezauth_server_map} ");
  $settings = array();
  foreach ($settings_info as $ss) {
    $desc = json_decode($ss->info);
    $settings[$ss->fa_field] = $desc->widget->type;
  }

  if (variable_get('formezauth_debug_log_access', 0)){
    watchdog('formezauth', 'Log accesso utente: %debug ', array('%debug' => json_encode($_SESSION['oauth_user_data'])), WATCHDOG_NOTICE);
  }

  // map email from server
  $edit['mail'] = $_SESSION['oauth_user_data']['mail'];
  $edit['status'] = $_SESSION['oauth_user_data']['status'];

  // uuid
  if ($edit['uid'] > 1){
    $edit['uuid'] = $_SESSION['oauth_user_data']['uuid'];
  }

  // check Profile2
  $profile_exists = (module_exists('profile2') && function_exists('profile2_load_by_user')) ? true : false;


  // Prende tutte le tipologie di profilo
  $entity_types = db_query("SELECT DISTINCT entity_type FROM {formezauth_fmap} WHERE local_field!='' AND local_field IS NOT NULL ");

  foreach ($entity_types as $etype) {

    // prende i dati di mappatura campi USER
    $result = db_query("SELECT fa_field, local_field, entity_type, col_default 
          FROM {formezauth_fmap} 
          WHERE local_field!='' 
          AND local_field IS NOT NULL 
          AND entity_type='" . $etype->entity_type . "'");

    if ($etype->entity_type == 'user'){

      foreach ($result as $row) {

        if ($row->fa_field == 'uid' || $row->fa_field == 'mail'){
          $edit[$row->fa_field] = $_SESSION['oauth_user_data'][$row->fa_field];
        } 
        else{

          if ($row->fa_field == '') continue;

          // identifica campi con i due punti
          $arr_fields = explode(":", $row->local_field, 2);
          $column = null;

          if (count($arr_fields) == 2 && $arr_fields[1] != 'format'){
            $field = $arr_fields[0];
            $column = $arr_fields[1];
          } 
          else{
            $field = $arr_fields[0];
          }

          // campi singoli
          if ($column == null){

            if ($row->fa_field == '__default'){

              $edit[$row->local_field][LANGUAGE_NONE][0]['value'] = $row->col_default;
            } 
            else if (isset($_SESSION['oauth_user_data'][$row->fa_field][LANGUAGE_NONE][0]['value'])){
                $edit[$row->local_field][LANGUAGE_NONE][0]['value'] = $_SESSION['oauth_user_data'][$row->fa_field][LANGUAGE_NONE][0]['value'];
            }
          }

          // campi composti, come address field
          else{

            if ($row->fa_field == '__default'){

              $edit[$field][LANGUAGE_NONE][0][$column] = $row->col_default;
            } 
            // Campi normali
            else if (isset($_SESSION['oauth_user_data'][$row->fa_field][LANGUAGE_NONE][0]['value'])){
              $edit[$field][LANGUAGE_NONE][0][$column] = $_SESSION['oauth_user_data'][$row->fa_field][LANGUAGE_NONE][0]['value'];
            }
            // Campi '' (vuoti)
            else if(isset($_SESSION['oauth_user_data'][$row->fa_field]) 
                    && is_array($_SESSION['oauth_user_data'][$row->fa_field])
                    && count($_SESSION['oauth_user_data'][$row->fa_field])==0){
              
              $edit[$field][LANGUAGE_NONE][0][$column]='';
            }
          }
        }
    }

      // https://auth.formez.eu/sites/default/files/styles/thumbnail/public/pictures/picture-22-1369253647.jpg	

      if (variable_get('formezauth_manage_image', '') != '' && $_SESSION['oauth_user_data']['picture']['filename'] != ''){

          $manage = true;
          $image = variable_get('formezauth_manage_image', '') . $_SESSION['oauth_user_data']['picture']['filename'];

          $file_location = system_retrieve_file($image, 'public://pictures', $manage);
          $files = file_load(db_query('SELECT MAX(fid) FROM {file_managed}')->fetchField());
          $picture_formezauth = new stdClass;
          $picture_formezauth->fid = $files->fid;
          $picture_formezauth->uid = $files->uid;
          $picture_formezauth->filename = $files->filename;
          $picture_formezauth->uri = $files->uri;
          $picture_formezauth->filemime = $files->filemime;
          $picture_formezauth->filesize = $files->filesize;
          $picture_formezauth->status = $files->status;
          $account->picture = $picture_formezauth;
      }

      // Save the account
      user_save($account, $edit);
    }


    // Profile fields
    else if ($profile_exists){

      $profile = profile2_load_by_user($account, $etype->entity_type);

      if (!isset($profile->pid)){

        $profile = profile2_create(array('user' => $user, 'type' => $etype->entity_type));
        profile2_save($profile);
      }

      foreach ($result as $row) {

        if (isset($_SESSION['oauth_user_data'][$row->fa_field][LANGUAGE_NONE][0]['value'])){

          $sess_val = $_SESSION['oauth_user_data'][$row->fa_field][LANGUAGE_NONE][0]['value'];
          $profile->{$row->local_field}[LANGUAGE_NONE][0]['value'] = $sess_val;
        }
      }
      
      // Salva il profilo
      profile2_save($profile);
      
    }
  }

  watchdog('formezauth', 'Login e aggiornamento utente %username', array('%username' => $user->name), WATCHDOG_INFO);

  unset($_SESSION['oauth_user_data']);

  drupal_alter('formezauth_user_login_post', $edit, $account);
}

/**
 * Implements hook_form_alter
 *
 * Se configurata la variabile, nasconde alcuni campi dalla modifica del profilo
 * ed impedisce di salvarlo (quindi di modificarlo) da client.
 * 
 * Serve per permettere una gestione degli utenti SOLO dal server FormezAuth
 * senza personalizzazione del client.
 * In questo caso è necessario far apparire un messaggio agli utenti per dire 
 * che si devono modificare il proprio profilo direttamente sul server.
 * @TODO Realizzare il messaggio per gli utenti
 */
function formezauth_form_alter(&$form, $form_state, $form_id) {

  if ($form_id == 'user_profile_form') {

    global $user;

    $serv_address = variable_get('formezauth_server_addr');

    $description_field = "La modifica di questo campo è disabilitata da questo sito. Per modificare questo campo è necessario accedere al proprio profilo su <a href=\"" . $serv_address . "\">" . $serv_address . "</a>";

    $formezauth_profile_policy = variable_get('formezauth_disable_profile_change', 1);

    // check user id
    if ($user->uid > 1) {

      // caso 2: disabilita i campi
      // @TODO: come disabilitare i campi di profile2?
      if ($formezauth_profile_policy == 2){

        drupal_set_message('La modifica del proprio profilo da questo sito è <strong>disabilitata</strong> e può essere effettuata da <a href="' . $serv_address . '/user">FormezAuth</a>', 'warning');

        unset($form['#submit']);
        unset($form['picture']);
        unset($form['account']);
        unset($form['locale']);
        $form['actions'] = array();

        foreach ($form as $ff => $field) {

          if (substr($ff, 0, 6) == 'field_'){
            unset($form[$ff]);
          }
        }

      } else if ($formezauth_profile_policy == 1){


        drupal_set_message('La modifica del proprio profilo da questo sito è <strong>limitata</strong>. Alcuni campi possono essere modificati da <a href="' . $serv_address . '/user">FormezAuth</a>', 'warning');

        $result = db_query("SELECT local_field, entity_type FROM {formezauth_fmap} 
					WHERE local_field!='' AND local_field IS NOT NULL ");

        foreach ($result as $row) {

          if (isset($form[$row->local_field])){
            $form[$row->local_field][LANGUAGE_NONE][0]['#disabled'] = TRUE;

            // @TODO questo non sembra funzionare
            $form[$row->local_field]['#description'] = $description_field;
            $form[$row->local_field][LANGUAGE_NONE]['#description'] = $description_field;
            $form[$row->local_field][LANGUAGE_NONE][0]['#description'] = $description_field;

          }
        }


      } else{
        
      }
    }

  }
}

/**
 * Funzione sostitutiva per la creazione dell'account
 * Sostituisce _connector_create_account per usare la variabile 'formezauth_account_create'
 * @see formezauth_connector_action_alter()
 * */
function _formezauth_create_account($connector_name, $cid = NULL, $consumer = NULL, $access_token = NULL, $request_token = NULL) {

  if ($connector_name != "oauthconnector_formezauth"){
    $fp = fopen('/tmp/falog', 'a');
    fwrite($fp, 'skip formezauth');
    fclose($fp);

    return _connector_create_account($connector_name, $cid, $consumer, $access_token, $request_token);
  }

  $fp = fopen('/tmp/falog', 'a');
  fwrite($fp, 'use formezauth');
  fclose($fp);



//  $fp=fopen('/tmp/fa.log','w');
//  fwrite($fp, "--ENTRATO--\n\n".json_encode($connector_name));
//  fclose($fp);


  $connector = _connector_get_connectors($connector_name);
  if (!$connector) {
    return FALSE;
  }
  $authname = $connector_name . '__' . $cid;

  if (variable_get('formezauth_account_create', FALSE) || variable_get('user_register', 1)) {
    $userinfo = array(
        'name' => $authname,
        'pass' => user_password(),
        'init' => $authname,
        'status' => variable_get('formezauth_account_create', FALSE) || variable_get('user_register', 1) == 1,
        'access' => REQUEST_TIME,
    );

    // Try to assign values from connection.
    $info = array();
    if (!empty($connector['information callback']) && is_callable($connector['information callback'])) {
      $info = $connector['information callback']($connector, $cid, array(), $access_token);
    }
    $allowed_fields = array('uuid', 'name', 'mail');

    /*
      $fp=fopen('/tmp/fa.log','a');
      fwrite($fp, "\nALLOWED: ". json_encode($allowed_fields));
      fwrite($fp, "\INFO: ". json_encode($info));
      fclose($fp);
     */

    foreach ($info as $field_name => $field) {
      if (in_array($field_name, $allowed_fields)) {


        $exists = db_select('users', 'u')
                ->fields('u')
                ->condition($field_name, $field['value'])
                ->execute()
                ->rowCount();


        /* $fp=fopen('/tmp/fa.log','a');
          fwrite($fp, "\nUFIND: ". json_encode(array(
          $field_name,
          $allowed_fields,
          $field['value'],
          $exists,
          )
          ));
          fclose($fp);
         */

        if ($exists == 1) {


          if ($field_name === 'uuid'){
            $userinfo = entity_uuid_load('user', array($field['value']));

            // $fp=fopen('/tmp/fa.log','a');
            // fwrite($fp, "\nUINFO: ". json_encode($userinfo));
            // fclose($fp);

            if ($userinfo !== false){
              break;
            }
          } else if ($field_name === 'name'){
            $userinfo = user_load_by_name($field['value']);
            if ($userinfo !== false){
              break;
            }
          } else if ($field_name === 'mail'){

            $userinfo = user_load_by_mail($field['value']);
            if ($userinfo !== false)
              break;
          }
          else if ($field_name === 'uid'){

            $userinfo = user_load($field['value']);
            if ($userinfo !== false)
              break;
          }

        }
        else{
          $userinfo[$field_name] = $field['value'];
        }
      }
    }



    // se non esiste l'account ne crea uno vuoto coin i dati minimi
    if (is_array($userinfo)){
      $new_account = user_save('', $userinfo);
    }


    // Terminate if an error occured during user_save().
    if (isset($userinfo->uid)){

      return $userinfo;
    } else if (!$new_account) {
      drupal_set_message(t("Error saving user account."), 'error');
    } else {
      watchdog('user', 'New external user: %name using module %module.', array('%name' => $authname, '%module' => 'connector'), WATCHDOG_NOTICE, l(t('edit'), 'user/' . $new_account->uid . '/edit'));
      return $new_account;
    }
  } else {
    drupal_set_message(t('Only site administrators can create new user accounts.'), 'error');
    if (isset($connector['logout callback']) && is_callable($connector['logout callback'])) {
      call_user_func($connector['logout callback'], $connector, $connection->cid);
    }
  }
}

