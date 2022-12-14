<?php

/**
 * @file
 * Defines a Workflow Field, Widget and Formatter for Workflow.
 *
 * All hooks are wrapper functions for a D8-style WorkflowDefaultWidget object.
 * The Widget is copied from options.module); the Formatter from list field.
 */

/**
 * Implements hook_field_widget_info().
 */
function workflowfield_field_widget_info() {
  return WorkflowDefaultWidget::settings();
}

/**
 * Implements hook_field_widget_settings_form().
 */
function workflowfield_field_widget_settings_form($field, $instance) {
  $form = array();
  $form_state = array();

  // The form element is created by a D8-like object.
  $widget = new WorkflowDefaultWidget($field, $instance);
  return $widget->settingsForm($form, $form_state, $has_data = 0);
}

/**
 * Implements hook_field_widget_form().
 *
 * param $langcode
 *   obsolete. Removed from formElement() parameter list.
 *
 * This is a wrapper function for the 'workflow form' Widget. $form is modified by reference.
 */
function workflowfield_field_widget_form(&$form, &$form_state, $field, $instance, $langcode, $items, $delta, $element) {
  if (!isset($element['#entity'])) {
    // We are now on the 'Manage Fields | Edit' settings page, so do nothing.
    return $element;
  }
  elseif (!isset($element['#entity_type'])) {
    // Not used: On an Entity View page, the widget is shown via
    // workflowfield_field_formatter_view() and workflow_transition_form().
    return $element;
  }
  else {
    // We are on an Entity add/edit page.
    if (!empty($field['settings']['widget']['hide'])) {
      // We want the widget to be hidden, when states are only to be
      // changed via Workflow History tab.
      // This setting is needed, because D7 core does't have a <hidden> widget.
      // @todo D8: remove this, since hidden widgets are available.
      return $element;
    }
    else {
      // Show the widget.
      $entity_type = $element['#entity_type'];
      $entity = $element['#entity'];

      if ($entity_type == 'comment') {
        // On a comment form, get the referenced entity.
        $entity_type = 'node'; // Comments are only possible on nodes.
        $entity = $form['#node'];
        // Make sure no action buttons are added.
        $instance['widget']['settings']['submit_function'] = array();
      }

      // Build a new $element.
      // Do not use backdrop_get_form() or you'll have a form in a form.
      $element = array();
      $widget = new WorkflowTransitionForm($field, $instance, $entity_type, $entity);
      $element = $widget->buildForm($element, $form_state);
      return $element;
    }
  }
  return $element;
}
