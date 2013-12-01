<?php
/**
 * @file
 * Provide views filter handler for workflow.module.
 */

/**
 * Filter by state.
 */
class workflow_views_handler_filter_sid extends views_handler_filter_in_operator {
  var $value_form_type = 'select';

  function get_value_options() {
    if (isset($this->value_options)) {
      return;
    }

    if (!isset($this->value_options)) {
      // Show the possible State options.
      $this->value_options = array();
      $this->value_title = t('Workflow state');

      $wid = $this->options['expose']['workflow_reduce_wid'];
      $all = $this->options['expose']['workflow_include_all'];

      $states = array();
      $grouped = FALSE;
      $workflows = Workflow::getWorkflows($wid);
      $count = count($workflows);
      if ($count > 1) {
        $states += array('' => t('No state'));
        $grouped = TRUE;
      }

      foreach ($workflows as $workflow) {
        // Only show the workflow name if more then 1 workflows exist.
        $states += $workflow->getOptions($grouped, $all);
      }
      $this->value_options = $states;
    }
    return $this->value_options;
  }

  function expose_options() {
    parent::expose_options();
    $this->options['expose']['workflow_reduce_wid'] = array('default' => 0);
    $this->options['expose']['workflow_include_all'] = FALSE;
  }

  function expose_form(&$form, &$form_state) {
    parent::expose_form($form, $form_state);

    $workflows[''] = t('- Select a value -');
    foreach (Workflow::getWorkflows() as $workflow) {
      $workflows[$workflow->wid] = $workflow->name;
    }

    $form['expose']['workflow_reduce_wid'] = array(
      '#type' => 'select',
      '#title' => t('Workflows'),
      '#options' => $workflows,
      '#description' => t('Select which workflow the states are given from.'),
      '#default_value' => $this->options['expose']['workflow_reduce_wid'],
    );
    $form['expose']['workflow_include_all'] = array(
      '#type' => 'checkbox',
      '#title' => "Include '(creation)' and inactive states?",
      '#default_value' => $this->options['expose']['workflow_include_all'],
    );
  }

  function option_definition() {
    $options = parent::option_definition();

    $options['expose']['contains']['workflow_reduce_wid'] = array('default' => '', 'bool' => FALSE);
    $options['expose']['contains']['workflow_include_all'] = array('default' => FALSE, 'bool' => TRUE);

    return $options;
  }

  function query() {
    $value = $this->is_a_group() && !$this->options['expose']['multiple'] ? drupal_array_merge_deep_array($this->value) : $this->value;
    if (empty($value)) {
      return;
    }

    $this->ensure_my_table();
    // $placeholder = !empty($this->definition['numeric']) ? '%d' : "'%s'";
    if (count($value) == 1) {
      $this->operator = ($this->operator == 'in') ? '= ' : '!= ';
      // $in = !empty($this->definition['numeric']) ? '%d' : "'%s'";
    }

    if ($this->operator == 'empty' || $this->operator == 'not empty') {
      $value = NULL;
      if ($this->operator == 'empty') {
        $this->operator = "IS NULL";
      }
      else {
        $this->operator = "IS NOT NULL";
      }
    }

    $this->query->add_where($this->options['group'], $this->table_alias . '.' . $this->real_field, $value, $this->operator);
  }
}