<?php

/**
 * @file
 * Contains workflow\includes\Entity\Workflow.
 */

class Workflow {
  // Since workflows do not change, it is implemented as a singleton.
  private static $workflows = array();

  public $wid = 0;
  public $name = '';
  public $tab_roles = array();
  public $options = array();
  protected $creation_sid = 0;
  protected $creation_state = NULL;
  // Helper for workflow_get_workflows_by_type().
  protected $item = NULL; // Helper to get/set the Item of a Workflow.

  /**
   * CRUD functions.
   */

  /**
   * Constructor.
   * The execution of the query instantiates objects and saves them in a static array.
   */
  protected function __construct($wid = 0) {
    if (!$wid) {
      // automatic constructor when casting an array or object.
      if (!is_array($this->options)) {
        $this->options = unserialize($this->options);
      }
      if ($this->wid) {
        self::$workflows[$this->wid] = $this;
      }
    }
    else {
      if (!isset(self::$workflows[$wid])) {
        self::$workflows[$wid] = Workflow::load($wid);
      }
      // Workflow may not exist.
      if (self::$workflows[$wid]) {
        // @todo: this copy-thing should not be necessary.
        $this->wid = self::$workflows[$wid]->wid;
        $this->name = self::$workflows[$wid]->name;
        $this->tab_roles = self::$workflows[$wid]->tab_roles;
        $this->options = self::$workflows[$wid]->options;
        $this->creation_sid = self::$workflows[$wid]->creation_sid;
      }
    }
  }

  /**
   * Creates and returns a new Workflow object.
   *
   * $param string $name
   *  The name of the new Workflow
   *
   * $return Workflow $workflow
   *  A new Workflow object
   *
   * "New considered harmful".
   */
  public static function create($name) {
    $workflow = new Workflow();
    $workflow->name = $name;
    return $workflow;
  }

  /**
   * Loads a Workflow object from table {workflows}
   * Implements a 'Factory' pattern to get Workflow objects from the database.
   *
   * $param string $wid
   *  The ID of the new Workflow
   *
   * $return Workflow $workflow
   *  A new Workflow object
   */
  public static function load($wid, $reset = FALSE) {
    $workflows = self::getWorkflows($wid, $reset);
    $workflow = isset($workflows[$wid]) ? $workflows[$wid] : NULL;
    return $workflow;
  }

  /**
   * Implements a 'Factory' pattern to get Workflow objects from the database.
   *
   * This is only called by CRUD functions in workflow.features.inc
   * More than likely in prep for an import / export action.
   * Therefore we don't want to fiddle with the response.
   * @deprecated: workflow_get_workflows_by_name() --> Workflow::getWorkflowByName($name)
   */
  public static function getWorkflowByName($name, $unserialize_options = FALSE) {
    foreach ($workflows = self::getWorkflows() as $workflow) {
      if ($name == $workflow->getName()) {
        if (!$unserialize_options) {
          $workflow->options = serialize($workflow->options);
        }
        return $workflow;
      }
    }
    return NULL;
  }

  /**
   * Returns an array of Workflows, reading them from table table {workflows}
   */
  public static function getWorkflows($wid = 0, $reset = FALSE) {
    if ($reset) {
      self::$workflows = array();
    }

    if ($wid && isset(self::$workflows[$wid])) {
      // Only 1 is requested and cached: return this one.
      return array($wid => self::$workflows[$wid]);
    }

    // Build the query.
    // If all are requested: read from db ($todo: cache this, but only used on Admin UI.)
    // If requested one is not cached: read from db.
    $query = db_select('workflows', 'w');
    $query->leftJoin('workflow_states', 'ws', 'w.wid = ws.wid');
    $query->fields('w');
    $query->addField('ws', 'sid', 'creation_sid');
    // Initially, read the Id of the creationState of the Workflow.
    $query->condition('ws.sysid', WORKFLOW_CREATION);

    $query->execute()->fetchAll(PDO::FETCH_CLASS, 'Workflow');

    // Return array of objects, even if only 1 is requested.
    // Note: self::workflows[] is populated in respective constructors.
    if ($wid > 0) {
      // return 1 object.
      $workflow = isset(self::$workflows[$wid]) ? self::$workflows[$wid] : NULL;
      return array($wid => $workflow);
    }
    else {
      return self::$workflows;
    }
  }

  /**
   * Given information, update or insert a new workflow.
   *
   * @deprecated: workflow_update_workflows() --> Workflow->save()
   */
  public function save($create_creation_state = TRUE) {
    $wid = $this->wid;

    if (isset($this->tab_roles) && is_array($this->tab_roles)) {
      $this->tab_roles = implode(',', $this->tab_roles);
    }
    if (is_array($this->options)) {
      $this->options = serialize($this->options);
    }

    if (($wid > 0) && Workflow::load($wid)) {
      drupal_write_record('workflows', $this, 'wid');
    }
    else {
      drupal_write_record('workflows', $this);
      if ($create_creation_state) {
        $creation_state = $this->getCreationState();
        $creation_state->save();
      }
    }
    // Update the page cache.
    self::$workflows[$wid] = $this;
  }

  /**
   * Given a wid, delete the workflow and its data.
   *
   * @deprecated: workflow_delete_workflows_by_wid() --> Workflow::delete().
   * @todo: This function does NOT delete WorkflowStates.
   */
  public function delete() {
    $wid = $this->wid;

    // Notify any interested modules before we delete the workflow.
    // E.g., Workflow Node deletes the {workflow_type_map} record.
    module_invoke_all('workflow', 'workflow delete', $wid, NULL, NULL, FALSE);

    // Delete associated state (also deletes any associated transitions).
    foreach ($this->getStates($all = TRUE) as $state) {
      $state->deactivate($new_sid = 0);
      $state->delete();
    }

    // Delete the workflow.
    db_delete('workflows')->condition('wid', $wid)->execute();
  }

  /**
   * Validate the workflow. Generate a message if not correct.
   *
   * This function is used on the settings page of:
   * - Workflow node: workflow_admin_ui_type_map_form()
   * - Workflow field: WorkflowItem->settingsForm()
   *
   * @return bool $is_valid
   */
  public function validate() {
    $is_valid = TRUE;

    // Don't allow workflows with no states. (There should always be a creation state.)
    $states = $this->getStates();
    if (count($states) < 2) {
      // That's all, so let's remind them to create some states.
      $message = t('%workflow has no states defined, so it cannot be assigned to content yet.',
        array('%workflow' => ucwords($this->getName())));
      drupal_set_message($message, 'warning');

      // Skip allowing this workflow.
      $is_valid = FALSE;
    }

    // Also check for transitions at least out of the creation state.
    // This always gets at least the "from" state.
    $transitions = workflow_allowable_transitions($this->getCreationSid(), 'to');
    if (count($transitions) < 2) {
      // That's all, so let's remind them to create some transitions.
      $message = t('%workflow has no transitions defined, so it cannot be assigned to content yet.',
        array('%workflow' => ucwords($this->getName())));
      drupal_set_message($message, 'warning');

      // Skip allowing this workflow.
      $is_valid = FALSE;
    }

    return $is_valid;
  }

  /**
   * Property functions.
   */

  /**
   * Gets the initial state for a newly created entity
   */
  public function getCreationState() {
    if (!isset($this->creation_state)) {
      $this->creation_state = WorkflowState::load($this->creation_sid);
    }
    if (!$this->creation_state) {
      $this->creation_state = WorkflowState::create($this->wid, '(creation)');
    }
    return $this->creation_state;
  }

  /**
   * Gets the ID of the initial state for a newly created entity.
   */
  public function getCreationSid() {
    return $this->creation_sid;
  }

  /**
   * Gets the first valid state ID, after the creation state.
   *
   * Use WorkflowState::getOptions(), because this does a access check.
   */
  public function getFirstSid($entity_type, $entity) {
    $creation_state = $this->getCreationState();
    $options = $creation_state->getOptions($entity_type, $entity);
    if ($options) {
      $keys = array_keys($options);
      $sid = $keys[0];
    }
    else {
      // This should never happen, but it did during testing.
      drupal_set_message(t('There are no workflow states available. Please notify your site administrator.'), 'error');
      $sid = 0;
    }
    return $sid;
  }

  /**
   * Gets all states for a given workflow.
   *
   * @param bool $all
   *   Indicates to return all (TRUE) or active (FALSE) states of a workflow.
   *
   * @return array $states
   *   An array of WorkflowState objects.
   */
  public function getStates($all = FALSE) {
    $states = WorkflowState::getStates($this->wid);
    if (!$all) {
      foreach ($states as $state) {
        if (!$state->isActive() && !$state->isCreationState()) {
          unset($states[$state->sid]);
        }
      }
    }
    return $states;
  }

  /**
   * Gets a state for a given workflow.
   *
   * @param $sid
   *   A state ID.
   *
   * @return
   *   A WorkflowState object.
   */
  public function getState($sid) {
    return WorkflowState::load($sid, $this->wid);
  }

  /**
   * Gets all valid transitions from a given state, in Options format.
   *
   * @param bool $grouped
   *   Indicates if the value must be grouped per workflow.
   *   This influence the rendering of the select_list options.
   *
   * @return array $options
   *   All states in a Workflow, as an array of $key => $label.
   */
  public function getOptions($grouped = FALSE) {
    $options = array();
    foreach ($this->getStates() as $state) {
      $options[$state->value()] = check_plain($state->label());
    }
    if ($grouped) {
      // Make a group for each Workflow.
      $label = check_plain($this->label());
      $grouped_options[$label] = $options;
      return $grouped_options;
    }
    return $options;
  }

  /**
   * Gets a setting from the state object.
   */
  public function getSetting($key, array $field = array()) {
    switch ($key) {
      case 'watchdog_log':
        if (isset($this->options['watchdog_log'])) {
          // This is set via Node API.
          return $this->options['watchdog_log'];
        }
        elseif ($field) {
          if (isset($field['settings']['watchdog_log'])) {
            // This is set via Field API.
            return $field['settings']['watchdog_log'];
          }
        }
        drupal_set_message('Setting Workflow::getSetting(' . $key . ') does not exist', 'error');
        break;

      default:
        drupal_set_message('Setting Workflow::getSetting(' . $key . ') does not exist', 'error');
    }
  }

  /**
   * Helper function for workflow_get_workflows_by_type()
   *
   * Get/set the Item of a particular Workflow.
   * It loads the Workflow object with the particular Field Instance data.
   * @todo: this is not robust: 1 Item has 1 Workflow; 1 Workflow may have N Items (fields)
   */
  public function getWorkflowItem(WorkflowItem $item = NULL) {
    if ($item) {
      $this->item = $item;
    }
    return $this->item;
  }

  /**
   * Mimics Entity API functions.
   */
  public function label($langcode = NULL) {
    return t($this->name, $args = array(), $options = array('langcode' => $langcode));
  }
  public  function getName() {
    return $this->name;
  }
  public function value() {
    return $this->wid;
  }

}
