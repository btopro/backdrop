<?php

/**
 * @file
 * User page callbacks for the book module.
 */

/**
 * Menu callback: Prints a listing of all books.
 *
 * @return string
 *   A HTML-formatted string with the listing of all books content.
 *
 * @see book_menu()
 */
function book_render() {
  $book_list = array();
  foreach (book_get_books() as $book) {
    $book_list[] = l($book['title'], $book['href'], $book['options']);
  }

  return theme('item_list', array('items' => $book_list));
}

/**
 * Page callback: Shows the outline form for a single node.
 *
 * @param Node $node
 *   The book node for which to show the outline.
 *
 * @return string
 *   A HTML-formatted string with the outline form for a single node.
 *
 * @see book_menu()
 */
function book_outline(Node $node) {
  backdrop_set_title($node->title);
  return backdrop_get_form('book_outline_form', $node);
}

/**
 * Form constructor for the book outline form.
 *
 * Allows handling of all book outline operations via the outline tab.
 *
 * @param Node $node
 *   The book node for which to show the outline.
 *
 * @see book_outline_form_submit()
 * @see book_remove_button_submit()
 * @ingroup forms
 */
function book_outline_form($form, &$form_state, Node $node) {
  if (!isset($node->book)) {
    // The node is not part of any book yet - set default options.
    $node->book = _book_link_defaults($node->nid);
  }
  else {
    $node->book['original_bid'] = $node->book['bid'];
  }

  // Find the depth limit for the parent select.
  if (!isset($node->book['parent_depth_limit'])) {
    $node->book['parent_depth_limit'] = _book_parent_depth_limit($node->book);
  }
  $form['#node'] = $node;
  $form['#id'] = 'book-outline';
  _book_add_form_elements($form, $form_state, $node);

  $form['book']['#collapsible'] = FALSE;

  $form['update'] = array(
    '#type' => 'submit',
    '#value' => $node->book['original_bid'] ? t('Update book outline') : t('Add to book outline'),
    '#weight' => 15,
  );

  $form['remove'] = array(
    '#type' => 'submit',
    '#value' => t('Remove from book outline'),
    '#access' => _book_node_is_removable($node),
    '#weight' => 20,
    '#submit' => array('book_remove_button_submit'),
  );

  return $form;
}

/**
 * Form submission handler for book_outline_form().
 *
 * Redirects to removal confirmation form.
 *
 * @see book_outline_form_submit()
 */
function book_remove_button_submit($form, &$form_state) {
  $form_state['redirect'] = 'node/' . $form['#node']->nid . '/outline/remove';
}

/**
 * Form submission handler for book_outline_form().
 *
 * @see book_remove_button_submit()
 */
function book_outline_form_submit($form, &$form_state) {
  $node = $form['#node'];
  $form_state['redirect'] = "node/" . $node->nid;
  $book_link = $form_state['values']['book'];
  if (!$book_link['bid']) {
    backdrop_set_message(t('No changes were made'));

    return;
  }

  $book_link['menu_name'] = book_menu_name($book_link['bid']);
  $node->book = $book_link;
  if (_book_update_outline($node)) {
    if ($node->book['parent_mismatch']) {
      // This will usually only happen when JS is disabled.
      backdrop_set_message(t('The post has been added to the selected book. You may now position it relative to other pages.'));
      $form_state['redirect'] = "node/" . $node->nid . "/outline";
    }
    else {
      backdrop_set_message(t('The book outline has been updated.'));
    }
    // Clear book_cache when book link is updated outside of node ops.
    _book_cache_clear($node->book['bid'], TRUE);
  }
  else {
    backdrop_set_message(t('There was an error adding the post to the book.'), 'error');
  }
}

/**
 * Form constructor to confirm removal of a node from a book.
 *
 * @param Node $node
 *   The node to delete.
 *
 * @see book_remove_form_submit()
 * @see book_menu()
 * @ingroup forms
 */
function book_remove_form($form, &$form_state, Node $node) {
  $form['#node'] = $node;
  $title = array('%title' => $node->title);

  if ($node->book['has_children']) {
    $description = t('%title has associated child pages, which will be relocated automatically to maintain their connection to the book. To recreate the hierarchy (as it was before removing this page), %title may be added again using the Outline tab, and each of its former child pages will need to be relocated manually.', $title);
  }
  else {
    $description = t('%title may be added to hierarchy again using the Outline tab.', $title);
  }

  return confirm_form($form, t('Are you sure you want to remove %title from the book hierarchy?', $title), 'node/' . $node->nid, $description, t('Remove'));
}

/**
 * Form submission handler for book_remove_form().
 */
function book_remove_form_submit($form, &$form_state) {
  $node = $form['#node'];
  if (_book_node_is_removable($node)) {
    menu_link_delete($node->book['mlid']);
    db_delete('book')
      ->condition('nid', $node->nid)
      ->execute();
    backdrop_set_message(t('The post has been removed from the book.'));
  }
  $form_state['redirect'] = 'node/' . $node->nid;
}
