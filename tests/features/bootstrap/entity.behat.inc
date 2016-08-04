<?php

/**
 * @file
 * Contains \EntitySubContext.
 */

use Acquia\LightningExtension\DrupalApiTrait;
use Behat\Gherkin\Node\TableNode;
use Drupal\Core\Entity\EntityInterface;
use Drupal\DrupalExtension\Context\DrupalSubContextBase;
use Drupal\DrupalExtension\Hook\Scope\EntityScope;
use Drupal\node\Entity\Node;
use Drupal\taxonomy\Entity\Term;
use Drupal\user\Entity\User;

/**
 * Subcontext for creating and cleaning up entities of any type.
 */
class EntitySubContext extends DrupalSubContextBase {

  use DrupalApiTrait;

  /**
   * Entities created during the scenario, organized by type.
   *
   * @var array
   */
  protected $entities = [];

  /**
   * The Mink context.
   *
   * @var \Drupal\DrupalExtension\Context\MinkContext
   */
  protected $minkContext;

  /**
   * Pre-scenario hook.
   *
   * @BeforeScenario
   */
  public function gatherContexts() {
    $this->minkContext = $this->getContext('\Drupal\DrupalExtension\Context\MinkContext');
  }

  /**
   * Creates a set of entities of a given type.
   *
   * @param string $entity_type
   *   The entity type to create.
   * @param \Behat\Gherkin\Node\TableNode $table
   *   The entities to create.
   *
   * @Given :entity_type entities:
   */
  public function createEntities($entity_type, TableNode $table) {
    $this->assertDrupalApi();

    foreach ($table as $row) {
      if ($entity_type == 'media' && empty($row['uid'])) {
        $row['uid'] = $this->getCurrentUid();
      }

      $entity = \Drupal::entityTypeManager()->getStorage($entity_type)->create($row);
      $entity->save();
      $this->queueEntity($entity);
    }
  }

  /**
   * Queues a node to be deleted at the end of the scenario.
   *
   * @param \Drupal\DrupalExtension\Hook\Scope\EntityScope $scope
   *   The hook scope.
   *
   * @afterNodeCreate
   */
  public function queueNode(EntityScope $scope) {
    $node = Node::load($scope->getEntity()->nid);
    $this->queueEntity($node);
  }

  /**
   * Queues a taxonomy term to be deleted at the end of the scenario.
   *
   * @param \Drupal\DrupalExtension\Hook\Scope\EntityScope $scope
   *   The hook scope.
   *
   * @afterTermCreate
   */
  public function queueTerm(EntityScope $scope) {
    $term = Term::load($scope->getEntity()->tid);
    $this->queueEntity($term);
  }

  /**
   * Queues a user to be deleted at the end of the scenario.
   *
   * @param \Drupal\DrupalExtension\Hook\Scope\EntityScope $scope
   *   The hook scope.
   *
   * @afterUserCreate
   */
  public function queueUser(EntityScope $scope) {
    $user = User::load($scope->getEntity()->uid);
    $this->queueEntity($user);
  }

  /**
   * Queues an entity to be deleted at the end of the scenario.
   *
   * @param \Drupal\Core\Entity\EntityInterface $entity
   *   The entity to queue.
   */
  public function queueEntity(EntityInterface $entity) {
    $entity_type = $entity->getEntityTypeId();
    $this->entities[$entity_type][] = $entity;
  }

  /**
   * Deletes all entities created during the scenario.
   *
   * @AfterScenario
   */
  public function cleanEntities() {
    foreach ($this->entities as $entity_type => $entities) {
      /** @var \Drupal\Core\Entity\EntityInterface $entity */
      foreach ($entities as $entity) {
        // Clean up the entity's alias, if there is one.
        $path = '/' . $entity->toUrl()->getInternalPath();
        $alias = \Drupal::service('path.alias_manager')->getAliasByPath($path);
        if ($alias != $path) {
          \Drupal::service('path.alias_storage')->delete(['alias' => $alias]);
        }

        $entity->delete();
      }
    }
  }

  /**
   * Visits a random entity of a specific type and bundle.
   *
   * @param string $entity_type
   *   The entity type ID.
   * @param string $bundle
   *   The bundle ID.
   *
   * @When I visit a :entity_type entity of type :bundle
   */
  public function visitEntity($entity_type, $bundle) {
    $this->assertDrupalApi();

    $bundle_key = \Drupal::entityTypeManager()
      ->getDefinition($entity_type)
      ->getKey('bundle');

    $entities = \Drupal::entityQuery($entity_type)
      ->condition($bundle_key, $bundle)
      ->execute();

    if ($entities) {
      $path = \Drupal::entityTypeManager()
        ->getStorage($entity_type)
        ->load(reset($entities))
        ->toUrl()
        ->getInternalPath();

      $this->visitPath($path);
    }
    else {
      throw new \UnexpectedValueException("No $entity_type entities found.");
    }
  }

  /**
   * Tracks the latest entities of a given type for post-scenario deletion.
   *
   * @param string $entity_type
   *   The entity type ID.
   * @param string $sort_field
   *   (optional) The creation time field to sort on. Defaults to 'created'.
   * @param int $limit
   *   (optional) The maximum number of entities to track. Defaults to 1.
   *
   * @When I queue the latest :entity_type entity for deletion
   */
  public function trackNewest($entity_type, $sort_field = 'created', $limit = 1) {
    $this->assertDrupalApi();

    $entities = \Drupal::entityQuery($entity_type)
      ->sort($sort_field, 'DESC')
      ->range(0, $limit)
      ->execute();

    $entities = \Drupal::entityTypeManager()
      ->getStorage($entity_type)
      ->loadMultiple($entities);

    array_walk($entities, [$this, 'queueEntity']);
  }

}