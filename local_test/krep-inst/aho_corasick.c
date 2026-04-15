/* aho_corasick.c - Memory-optimized implementation of Aho-Corasick algorithm
 *
 * This implementation uses a more memory-efficient representation for trie nodes
 * to reduce the 2KB overhead per node (256 pointers x 8 bytes on 64-bit systems).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h> // Include for tolower

#include "krep.h"         // Include main header FIRST for search_params_t definition
#include "aho_corasick.h" // Include the header defining ac_trie_t forward decl

typedef struct ac_node
{
    struct ac_node *children[256]; // Child nodes for each character
    struct ac_node *fail_link;     // Failure link for state transitions
    size_t *output_indices;        // Array of pattern indices ending at this node
    int num_outputs;               // Number of pattern indices ending exactly here
    int capacity_outputs;          // Capacity of the output_indices array
} ac_node_t;

struct ac_trie
{
    ac_node_t *root;     // Root node of the trie
    size_t num_patterns; // Number of patterns in the trie
    bool case_sensitive; // Whether search is case-sensitive
};

// Create a new AC node
static ac_node_t *ac_node_create()
{
    fprintf(stderr, "[aho_corasick.c] enter ac_node_create 1\n");
    ac_node_t *node = calloc(1, sizeof(ac_node_t));
    if (!node)
    {
        perror("Failed to allocate memory for Aho-Corasick node");
        return NULL;
    // fprintf(stderr, "[aho_corasick.c] exit ac_node_create 1\n");
    }
    
    fprintf(stderr, "[aho_corasick.c] enter ac_node_create 2\n");
    // Initialize children array to NULL
    memset(node->children, 0, sizeof(node->children));

    // Initialize output_indices as NULL (allocated later if needed)
    node->output_indices = NULL;
    node->num_outputs = 0;
    node->capacity_outputs = 0;
    node->fail_link = NULL;

    return node;
    // fprintf(stderr, "[aho_corasick.c] exit ac_node_create 2\n");
}

// Add pattern index to node outputs
static bool ac_node_add_output(ac_node_t *node, size_t pattern_index)
{
    fprintf(stderr, "[aho_corasick.c] enter ac_node_add_output 1\n");
    if (!node)
        return false;
    // fprintf(stderr, "[aho_corasick.c] exit ac_node_add_output 1\n");

    // Check if we need to allocate or resize
    if (node->num_outputs >= node->capacity_outputs)
    {
        fprintf(stderr, "[aho_corasick.c] enter ac_node_add_output 2\n");
        int new_capacity = node->capacity_outputs == 0 ? 4 : node->capacity_outputs * 2;
        // Check for potential integer overflow before multiplication
        if (new_capacity < node->capacity_outputs)
        {
            fprintf(stderr, "[aho_corasick.c] enter ac_node_add_output 3\n");
            perror("Integer overflow calculating new capacity for Aho-Corasick node outputs");
            return false;
            // fprintf(stderr, "[aho_corasick.c] exit ac_node_add_output 3\n");
        }
        
        fprintf(stderr, "[aho_corasick.c] enter ac_node_add_output 4\n");
        size_t *new_outputs = realloc(node->output_indices, new_capacity * sizeof(size_t));
        if (!new_outputs)
        {
            fprintf(stderr, "[aho_corasick.c] enter ac_node_add_output 5\n");
            perror("Failed to resize Aho-Corasick node outputs");
            return false;
            // fprintf(stderr, "[aho_corasick.c] exit ac_node_add_output 5\n");
        }
        
        fprintf(stderr, "[aho_corasick.c] enter ac_node_add_output 6\n");
        node->output_indices = new_outputs;
        node->capacity_outputs = new_capacity;
        // fprintf(stderr, "[aho_corasick.c] exit ac_node_add_output 6\n");
        // fprintf(stderr, "[aho_corasick.c] exit ac_node_add_output 4\n");
        // fprintf(stderr, "[aho_corasick.c] exit ac_node_add_output 2\n");
    }

    fprintf(stderr, "[aho_corasick.c] enter ac_node_add_output 7\n");
    // Add the pattern index
    node->output_indices[node->num_outputs++] = pattern_index;
    return true;
    // fprintf(stderr, "[aho_corasick.c] exit ac_node_add_output 7\n");
}

// Free an AC node and all its children recursively
static void ac_node_free(ac_node_t *node)
{
    fprintf(stderr, "[aho_corasick.c] enter ac_node_free 1\n");
    if (!node)
        return;
    // fprintf(stderr, "[aho_corasick.c] exit ac_node_free 1\n");

    fprintf(stderr, "[aho_corasick.c] enter ac_node_free 2\n");
    // Recursively free children
    for (int i = 0; i < 256; i++)
    {
        fprintf(stderr, "[aho_corasick.c] enter ac_node_free 3\n");
        if (node->children[i])
        {
            fprintf(stderr, "[aho_corasick.c] enter ac_node_free 4\n");
            ac_node_free(node->children[i]);
            // fprintf(stderr, "[aho_corasick.c] exit ac_node_free 4\n");
        }
        // fprintf(stderr, "[aho_corasick.c] exit ac_node_free 3\n");
    }

    // Free outputs array if allocated
    if (node->output_indices)
    {
        fprintf(stderr, "[aho_corasick.c] enter ac_node_free 5\n");
        free(node->output_indices);
        // fprintf(stderr, "[aho_corasick.c] exit ac_node_free 5\n");
    }

    // Free the node itself
    free(node);
    // fprintf(stderr, "[aho_corasick.c] exit ac_node_free 2\n");
}

// Build the Aho-Corasick Trie
ac_trie_t *ac_trie_build(const search_params_t *params)
{
    fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 1\n");
    if (!params || params->num_patterns == 0)
    {
        return NULL;
    // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 1\n");
    }

    fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 2\n");
    // Allocate the trie structure
    ac_trie_t *trie = malloc(sizeof(ac_trie_t));
    if (!trie)
    {
        fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 3\n");
        perror("Failed to allocate Aho-Corasick trie");
        return NULL;
        // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 3\n");
    }

    fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 4\n");
    // Create the root node
    trie->root = ac_node_create();
    if (!trie->root)
    {
        fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 5\n");
        free(trie);
        return NULL;
        // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 5\n");
    }
    
    fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 6\n");
    trie->root->fail_link = trie->root; // Root's failure link points to itself

    trie->num_patterns = params->num_patterns;
    trie->case_sensitive = params->case_sensitive;
    // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 6\n");

    // Build the trie - Insert all patterns
    for (size_t p = 0; p < params->num_patterns; p++)
    {
        fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 7\n");
        const unsigned char *pattern = (const unsigned char *)params->patterns[p];
        size_t pattern_len = params->pattern_lens[p];
        // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 7\n");

        // Handle empty pattern: Add index 0 to root's output if an empty pattern exists
        if (pattern_len == 0)
        {
            fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 8\n");
            if (!ac_node_add_output(trie->root, p))
            {
                fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 9\n");
                ac_trie_free(trie);
                return NULL;
                // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 9\n");
            }
            continue;
            // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 8\n");
        }

        fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 10\n");
        ac_node_t *current = trie->root;
        // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 10\n");

        // Insert each character of the pattern
        for (size_t i = 0; i < pattern_len; i++)
        {
            fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 11\n");
            unsigned char c_orig = pattern[i];
            unsigned char c = params->case_sensitive ? c_orig : lower_table[c_orig];
            // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 11\n");

            // Create child node if it doesn't exist
            if (!current->children[c])
            {
                fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 12\n");
                current->children[c] = ac_node_create();
                if (!current->children[c])
                {
                    fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 13\n");
                    // Handle allocation failure
                    ac_trie_free(trie);
                    return NULL;
                    // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 13\n");
                }
                // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 12\n");
            }

            fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 14\n");
            // Advance to the child
            current = current->children[c];
            // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 14\n");
        }

        fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 15\n");
        // Mark this node with the pattern index (used later for output)
        if (!ac_node_add_output(current, p))
        {
            fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 16\n");
            ac_trie_free(trie);
            return NULL;
            // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 16\n");
        }
        // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 15\n");
    }

    fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 17\n");
    // --- Build failure links using BFS with dynamic queue ---
    size_t queue_capacity = 64; // Initial capacity
    ac_node_t **queue = malloc(queue_capacity * sizeof(ac_node_t *));
    if (!queue)
    {
        fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 18\n");
        perror("Failed to allocate initial queue for Aho-Corasick BFS");
        ac_trie_free(trie);
        return NULL;
        // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 18\n");
    }

    fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 19\n");
    size_t queue_front = 0;
    size_t queue_rear = 0;
    // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 19\n");

    // Add root's immediate children to the queue and set their failure links to root
    for (int c_val = 0; c_val < 256; c_val++) // Use c_val to avoid shadowing
    {
        fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 20\n");
        unsigned char c = (unsigned char)c_val; // Cast for indexing
        // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 20\n");
        if (trie->root->children[c])
        {
            fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 21\n");
            ac_node_t *child = trie->root->children[c];
            child->fail_link = trie->root; // Direct children fail to root
            // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 21\n");

            // Enqueue
            if (queue_rear >= queue_capacity)
            {
                fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 22\n");
                queue_capacity *= 2;
                ac_node_t **new_queue = realloc(queue, queue_capacity * sizeof(ac_node_t *));
                if (!new_queue)
                {
                    fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 23\n");
                    perror("Failed to resize queue for Aho-Corasick BFS");
                    free(queue);
                    ac_trie_free(trie);
                    return NULL;
                    // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 23\n");
                }
                
                fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 24\n");
                queue = new_queue;
                // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 24\n");
                // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 22\n");
            }
            
            fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 25\n");
            queue[queue_rear++] = child;
            // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 25\n");
        }
    }

    // BFS to build failure links
    while (queue_front < queue_rear)
    {
        fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 26\n");
        ac_node_t *current = queue[queue_front++]; // Dequeue
        // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 26\n");

        // Process each child of current node
        for (int c_val = 0; c_val < 256; c_val++) // Use c_val to avoid shadowing
        {
            fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 27\n");
            unsigned char c = (unsigned char)c_val; // Cast to unsigned char for indexing
            // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 27\n");
            if (current->children[c])
            {
                fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 28\n");
                ac_node_t *child = current->children[c];
                // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 28\n");

                // Enqueue child
                if (queue_rear >= queue_capacity)
                {
                    fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 29\n");
                    queue_capacity *= 2;
                    ac_node_t **new_queue = realloc(queue, queue_capacity * sizeof(ac_node_t *));
                    if (!new_queue)
                    {
                        fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 30\n");
                        perror("Failed to resize queue during Aho-Corasick BFS");
                        free(queue);
                        ac_trie_free(trie);
                        return NULL;
                        // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 30\n");
                    }
                    
                    fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 31\n");
                    queue = new_queue;
                    // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 31\n");
                    // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 29\n");
                }
                
                fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 32\n");
                queue[queue_rear++] = child;
                // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 32\n");

                // Find failure link for this child
                ac_node_t *failure = current->fail_link;
                while (failure != trie->root && !failure->children[c])
                {
                    fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 33\n");
                    failure = failure->fail_link;
                    // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 33\n");
                }

                fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 34\n");
                // Set the failure link
                child->fail_link = failure->children[c] ? failure->children[c] : trie->root;
                // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 34\n");
            }
        }
    }

    fprintf(stderr, "[aho_corasick.c] enter ac_trie_build 35\n");
    free(queue);
    return trie;
    // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 35\n");
    // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 17\n");
    // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 4\n");
    // fprintf(stderr, "[aho_corasick.c] exit ac_trie_build 2\n");
}

// Free the Aho-Corasick Trie
void ac_trie_free(ac_trie_t *trie)
{
    fprintf(stderr, "[aho_corasick.c] enter ac_trie_free 1\n");
    if (!trie)
        return;
    // fprintf(stderr, "[aho_corasick.c] exit ac_trie_free 1\n");

    fprintf(stderr, "[aho_corasick.c] enter ac_trie_free 2\n");
    // Free the root node and all its children
    ac_node_free(trie->root);

    // Free the trie structure
    free(trie);
    // fprintf(stderr, "[aho_corasick.c] exit ac_trie_free 2\n");
}

// Check if the root node has outputs (used for empty pattern matching in empty files)
bool ac_trie_root_has_outputs(const ac_trie_t *trie)
{
    fprintf(stderr, "[aho_corasick.c] enter ac_trie_root_has_outputs 1\n");
    return (trie != NULL && trie->root != NULL && trie->root->num_outputs > 0);
    // fprintf(stderr, "[aho_corasick.c] exit ac_trie_root_has_outputs 1\n");
}

// Forward declarations for helper functions (assuming they exist in krep.c or elsewhere)
extern size_t find_line_start(const char *text, size_t max_len, size_t pos);
extern bool is_whole_word_match(const char *text, size_t text_len, size_t start, size_t end);
// Match the declaration in krep.h (remove const)
extern unsigned char lower_table[256];

// Corrected Aho-Corasick search function
uint64_t aho_corasick_search(const search_params_t *params,
                             const char *text_start,
                             size_t text_len,
                             match_result_t *result)
{
    fprintf(stderr, "[aho_corasick.c] enter aho_corasick_search 1\n");
    // --- 1. Validations ---
    // Check for NULL pointers for essential structures
    if (!params || !params->ac_trie || !text_start)
        return 0;
    // fprintf(stderr, "[aho_corasick.c] exit aho_corasick_search 1\n");
    
    fprintf(stderr, "[aho_corasick.c] enter aho_corasick_search 2\n");
    // Add check for root node existence
    if (!params->ac_trie->root)
    {
        fprintf(stderr, "[aho_corasick.c] enter aho_corasick_search 3\n");
        // Trie structure exists but root is NULL, indicates build failure
        fprintf(stderr, "Warning: Aho-Corasick trie root is NULL during search.\n");
        return 0;
        // fprintf(stderr, "[aho_corasick.c] exit aho_corasick_search 3\n");
    }
    
    fprintf(stderr, "[aho_corasick.c] enter aho_corasick_search 4\n");
    // If max_count is 0, no matches should be found.
    if (params->max_count == 0)
        return 0;
    // fprintf(stderr, "[aho_corasick.c] exit aho_corasick_search 4\n");

    fprintf(stderr, "[aho_corasick.c] enter aho_corasick_search 5\n");
    ac_trie_t *trie = params->ac_trie;
    ac_node_t *current_node = trie->root;
    uint64_t matches_found = 0;
    const size_t max_count = params->max_count; // Use const for clarity
    const bool count_lines_mode = params->count_lines_mode;
    const bool track_positions = params->track_positions;
    size_t last_counted_line_start = SIZE_MAX; // Used only if count_lines_mode is true
    // fprintf(stderr, "[aho_corasick.c] exit aho_corasick_search 5\n");

    // --- 2. Iterate through the text ---
    for (size_t i = 0; i < text_len; i++)
    {
        fprintf(stderr, "[aho_corasick.c] enter aho_corasick_search 6\n");
        // --- 3. Get character and handle case-insensitivity ---
        unsigned char c_orig = (unsigned char)text_start[i];
        // Use lower_table for case-insensitive matching during search
        unsigned char c = params->case_sensitive ? c_orig : lower_table[c_orig];
        // fprintf(stderr, "[aho_corasick.c] exit aho_corasick_search 6\n");

        // --- 4. Follow failure links ---
        // Traverse failure links until a node with a transition for 'c' is found,
        // or until the root node is reached.
        while (current_node != trie->root && !current_node->children[c])
        {
            fprintf(stderr, "[aho_corasick.c] enter aho_corasick_search 7\n");
            current_node = current_node->fail_link;
            // fprintf(stderr, "[aho_corasick.c] exit aho_corasick_search 7\n");
        }

        // --- 5. Make state transition ---
        // If a transition for 'c' exists from the current node (or a node reached via failure links),
        // move to that child node. Otherwise, stay at the root.
        if (current_node->children[c])
        {
            fprintf(stderr, "[aho_corasick.c] enter aho_corasick_search 8\n");
            current_node = current_node->children[c];
            // fprintf(stderr, "[aho_corasick.c] exit aho_corasick_search 8\n");
        }
        // If no transition exists even from the root, current_node remains root for the next character.

        // --- 6. Collect matches by following failure links from the current state ---
        ac_node_t *output_node = current_node;
        // Check the current node and all nodes reachable via failure links for outputs.
        // This ensures we find all patterns ending at the current position 'i'.
        while (output_node != trie->root)
        {
            fprintf(stderr, "[aho_corasick.c] enter aho_corasick_search 9\n");
            if (output_node->num_outputs > 0)
            {
                fprintf(stderr, "[aho_corasick.c] enter aho_corasick_search 10\n");
                // --- 7. Process patterns ending at this output_node ---
                for (int j = 0; j < output_node->num_outputs; j++)
                {
                    fprintf(stderr, "[aho_corasick.c] enter aho_corasick_search 11\n");
                    // Check max_count *before* processing this specific match.
                    // If we've already reached the limit, return immediately.
                    if (matches_found >= max_count)
                    {
                        fprintf(stderr, "[aho_corasick.c] enter aho_corasick_search 12\n");
                        return matches_found;
                        // fprintf(stderr, "[aho_corasick.c] exit aho_corasick_search 12\n");
                    }

                    fprintf(stderr, "[aho_corasick.c] enter aho_corasick_search 13\n");
                    size_t pattern_idx = output_node->output_indices[j];
                    size_t pattern_len = params->pattern_lens[pattern_idx];
                    // fprintf(stderr, "[aho_corasick.c] exit aho_corasick_search 13\n");

                    // Skip empty patterns (should not happen if build logic is correct, but safe check)
                    if (pattern_len == 0)
                    {
                        fprintf(stderr, "[aho_corasick.c] enter aho_corasick_search 14\n");
                        continue;
                        // fprintf(stderr, "[aho_corasick.c] exit aho_corasick_search 14\n");
                    }

                    fprintf(stderr, "[aho_corasick.c] enter aho_corasick_search 15\n");
                    // --- 8. Calculate match position ---
                    // Match ends *at* index i (inclusive).
                    // Start index is i - pattern_len + 1.
                    size_t match_start = i + 1 - pattern_len;
                    size_t match_end = i + 1; // Exclusive end position
                    // fprintf(stderr, "[aho_corasick.c] exit aho_corasick_search 15\n");

                    // --- 9. Apply whole word check ---
                    if (params->whole_word && !is_whole_word_match(text_start, text_len, match_start, match_end))
                    {
                        fprintf(stderr, "[aho_corasick.c] enter aho_corasick_search 16\n");
                        continue; // Skip if not a whole word match
                        // fprintf(stderr, "[aho_corasick.c] exit aho_corasick_search 16\n");
                    }

                    // --- 10. Process match based on mode ---
                    if (count_lines_mode)
                    {
                        fprintf(stderr, "[aho_corasick.c] enter aho_corasick_search 17\n");
                        size_t line_start = find_line_start(text_start, text_len, match_start);
                        // Only count if this line hasn't been counted yet for this search
                        if (line_start != last_counted_line_start)
                        {
                            fprintf(stderr, "[aho_corasick.c] enter aho_corasick_search 18\n");
                            // We already checked max_count at the start of the inner loop
                            matches_found++;
                            last_counted_line_start = line_start;
                            // Check again immediately after incrementing in case this was the last one needed
                            if (matches_found >= max_count)
                            {
                                fprintf(stderr, "[aho_corasick.c] enter aho_corasick_search 19\n");
                                return matches_found;
                                // fprintf(stderr, "[aho_corasick.c] exit aho_corasick_search 19\n");
                            }
                            // fprintf(stderr, "[aho_corasick.c] exit aho_corasick_search 18\n");
                        }
                        // fprintf(stderr, "[aho_corasick.c] exit aho_corasick_search 17\n");
                    }
                    else // Count matches or track positions
                    {
                        fprintf(stderr, "[aho_corasick.c] enter aho_corasick_search 20\n");
                        // We already checked max_count at the start of the inner loop
                        matches_found++;
                        // fprintf(stderr, "[aho_corasick.c] exit aho_corasick_search 20\n");

                        // Add position if tracking is enabled and result struct is provided
                        if (track_positions && result)
                        {
                            fprintf(stderr, "[aho_corasick.c] enter aho_corasick_search 21\n");
                            match_result_add(result, match_start, match_end);
                            // fprintf(stderr, "[aho_corasick.c] exit aho_corasick_search 21\n");
                        }
                        
                        fprintf(stderr, "[aho_corasick.c] enter aho_corasick_search 22\n");
                        // Check again immediately after incrementing
                        if (matches_found >= max_count)
                        {
                            fprintf(stderr, "[aho_corasick.c] enter aho_corasick_search 23\n");
                            return matches_found;
                            // fprintf(stderr, "[aho_corasick.c] exit aho_corasick_search 23\n");
                        }
                        // fprintf(stderr, "[aho_corasick.c] exit aho_corasick_search 22\n");
                    }
                    // fprintf(stderr, "[aho_corasick.c] exit aho_corasick_search 11\n");
                } // End loop through outputs at this node
                // fprintf(stderr, "[aho_corasick.c] exit aho_corasick_search 10\n");
            }

            fprintf(stderr, "[aho_corasick.c] enter aho_corasick_search 24\n");
            // --- 11. Follow failure link ---
            // Move to the failure link node to find shorter patterns ending at the same position 'i'.
            output_node = output_node->fail_link;
            // fprintf(stderr, "[aho_corasick.c] exit aho_corasick_search 24\n");

            // Optimization: Check max_count again before continuing the inner while loop.
            // If the limit was reached while processing outputs of the current output_node,
            // we can stop checking further failure links for this text position 'i'.
            if (matches_found >= max_count)
            {
                fprintf(stderr, "[aho_corasick.c] enter aho_corasick_search 25\n");
                return matches_found;
                // fprintf(stderr, "[aho_corasick.c] exit aho_corasick_search 25\n");
            }
            // fprintf(stderr, "[aho_corasick.c] exit aho_corasick_search 9\n");
        } // End while(output_node != trie->root)

        // Check max_count one last time after processing all outputs for position `i`
        if (matches_found >= max_count)
        {
            fprintf(stderr, "[aho_corasick.c] enter aho_corasick_search 26\n");
            return matches_found;
            // fprintf(stderr, "[aho_corasick.c] exit aho_corasick_search 26\n");
        }
        // fprintf(stderr, "[aho_corasick.c] exit aho_corasick_search 2\n");
    } // End loop through text (for size_t i = 0; ...)

    fprintf(stderr, "[aho_corasick.c] enter aho_corasick_search 27\n");
    // --- Handle potential empty pattern match in empty text ---
    // This case is only relevant if the input text itself was empty.
    // An empty pattern ("") should match an empty text exactly once if present.
    if (text_len == 0 && trie->root->num_outputs > 0)
    {
        fprintf(stderr, "[aho_corasick.c] enter aho_corasick_search 28\n");
        for (int j = 0; j < trie->root->num_outputs; ++j)
        {
            fprintf(stderr, "[aho_corasick.c] enter aho_corasick_search 29\n");
            size_t pattern_idx = trie->root->output_indices[j];
            // Check if this output corresponds to the empty pattern
            if (params->pattern_lens[pattern_idx] == 0)
            {
                fprintf(stderr, "[aho_corasick.c] enter aho_corasick_search 30\n");
                // Found the empty pattern match at the root
                if (matches_found < max_count) // Should always be true if text_len is 0 unless max_count was 0
                {
                    fprintf(stderr, "[aho_corasick.c] enter aho_corasick_search 31\n");
                    matches_found++;
                    if (track_positions && result)
                    {
                        fprintf(stderr, "[aho_corasick.c] enter aho_corasick_search 32\n");
                        match_result_add(result, 0, 0); // Empty match at position 0
                        // fprintf(stderr, "[aho_corasick.c] exit aho_corasick_search 32\n");
                    }
                    // fprintf(stderr, "[aho_corasick.c] exit aho_corasick_search 31\n");
                }
                
                fprintf(stderr, "[aho_corasick.c] enter aho_corasick_search 33\n");
                // Only count one match for the empty pattern in empty text
                break;
                // fprintf(stderr, "[aho_corasick.c] exit aho_corasick_search 33\n");
                // fprintf(stderr, "[aho_corasick.c] exit aho_corasick_search 30\n");
            }
            // fprintf(stderr, "[aho_corasick.c] exit aho_corasick_search 29\n");
        }
        // fprintf(stderr, "[aho_corasick.c] exit aho_corasick_search 28\n");
    }

    return matches_found;
    // fprintf(stderr, "[aho_corasick.c] exit aho_corasick_search 27\n");
}
// Total cost: 0.170216
// Total split cost: 0.000000, input tokens: 0, output tokens: 0, cache read tokens: 0, cache write tokens: 0, split chunks: [(0, 466)]
// Total instrumented cost: 0.170216, input tokens: 7046, output tokens: 8178, cache read tokens: 0, cache write tokens: 7042
