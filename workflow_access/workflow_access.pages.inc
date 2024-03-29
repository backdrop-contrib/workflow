<?php
/**
 * @file
 * Provides pages for administrative UI.
 */

/**
 * Implements hook_form().
 *
 * Add a form to set the weight fo the access module.
 */
function workflow_access_priority_form($form, $form_state) {
  $form['workflow_access'] = array(
    '#type' => 'fieldset',
    '#title' => t('Workflow Access Settings'),
    '#collapsible' => FALSE,
    '#collapsed' => FALSE,
  );
  $form['workflow_access']['#tree'] = TRUE;
  $form['workflow_access']['workflow_access_priority'] = array(
    '#type' => 'weight',
    '#delta' => 10,
    '#title' => t('Workflow Access Priority'),
    '#default_value' => config_get('workflow_access.settings', 'workflow_access_priority'),
    '#description' => t('This sets the node access priority. Changing this
      setting can be dangerous. If there is any doubt, leave it at 0.
      <a href="@url">Read the manual.</a>', array('@url' => url('https://api.drupal.org/api/backdrop/modules!node!node.api.php/function/hook_node_access_records/7'))),
  );
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
  );

  return $form;
}

/**
 * Submit handler.
 */
function workflow_access_priority_form_submit($form, &$form_state) {
  config_set('workflow_access.settings', 'workflow_access_priority', $form_state['values']['workflow_access']['workflow_access_priority']);
  $form_state['redirect'] = WORKFLOW_ADMIN_UI_PATH;
}


/**
 * Implements hook_form().
 *
 * Add a "three dimensional" (state, role, permission type) configuration
 * interface to the workflow edit form.
 */
function workflow_access_form($form, $form_state, Workflow $workflow) {
  // If we don't have a workflow that goes with this, return to the admin pg.
  if (!is_object($workflow)) {
    backdrop_set_message(t('Improper worklow ID provided.'), 'error');
    backdrop_goto(WORKFLOW_ADMIN_UI_PATH);
  }

  // @todo: Let's get a better page title.
  backdrop_set_title(t('@name Access', array('@name' => $workflow->label())));

  $form['#tree'] = TRUE;
  $form['#wid'] = $wid = $workflow->getWorkflowId();

  // A list of role names keyed by role ID, including the 'author' role.
  $roles = workflow_get_roles('participate in workflow');

  // Add a table for every workflow state.
  foreach ($workflow->getStates($all = TRUE) as $state) {
    if ($state->isCreationState()) {
      // No need to set perms on creation.
      continue;
    }
    $view = $update = $delete = array();
    $count = 0;
    foreach (workflow_access_get_workflow_access_by_sid($state->sid) as $access) {
      $count++;
      if ($access->grant_view) {
        $view[] = $access->gid;
      }
      if ($access->grant_update) {
        $update[] = $access->gid;
      }
      if ($access->grant_delete) {
        $delete[] = $access->gid;
      }
    }
    // Allow view grants by default for anonymous and authenticated users,
    // if no grants were set up earlier.
    // @todo can't use role names for grant ID.
    if (!$count) {
      $view = array(BACKDROP_ANONYMOUS_ROLE, BACKDROP_AUTHENTICATED_ROLE);
    }
    // @todo: better tables using a #theme function instead of direct #prefixing.
    $form[$state->sid] = array(
      '#type' => 'fieldset',
      '#title' => $state->label(),
      '#collapsible' => TRUE,
      '#collapsed' => FALSE,
      '#tree' => TRUE,
    );

    $form[$state->sid]['view'] = array(
      '#type' => 'checkboxes',
      '#options' => $roles,
      '#default_value' => $view,
      '#title' => t('Roles who can view posts in this state'),
      '#prefix' => '<table width="100%" style="border: 0;"><tbody style="border: 0;"><tr><td>',
    );

    $form[$state->sid]['update'] = array(
      '#type' => 'checkboxes',
      '#options' => $roles,
      '#default_value' => $update,
      '#title' => t('Roles who can edit posts in this state'),
      '#prefix' => "</td><td>",
    );

    $form[$state->sid]['delete'] = array(
      '#type' => 'checkboxes',
      '#options' => $roles,
      '#default_value' => $delete,
      '#title' => t('Roles who can delete posts in this state'),
      '#prefix' => "</td><td>",
      '#suffix' => "</td></tr></tbody></table>",
    );
  }

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save configuration'),
  );

  return $form;
}

/**
 * Stores permission settings for workflow states.
 */
function workflow_access_form_submit($form, &$form_state) {
  $wid = $form['#wid'];

  foreach ($form_state['values'] as $sid => $access) {
    // Ignore irrelevant keys.
    if (!is_numeric($sid)) {
      continue;
    }
    foreach ($access['view'] as $role => $checked) {
      $data = array(
        'sid' => $sid,
        'role' => $role,
        'grant_view' => (!empty($checked)) ? (bool) $checked : 0,
        'grant_update' => (!empty($access['update'][$role])) ? (bool) $access['update'][$role] : 0,
        'grant_delete' => (!empty($access['delete'][$role])) ? (bool) $access['delete'][$role] : 0,
      );
      workflow_access_insert_workflow_access_by_sid($data);
    }

    // Update all nodes having same workflow state to reflect new settings.
    // just set a flag, which is working for both Workflow Field ánd Workflow Node.
    node_access_needs_rebuild(TRUE);
  }

  backdrop_set_message(t('Workflow access permissions updated.'));
  $form_state['redirect'] = WORKFLOW_ADMIN_UI_PATH;
}
