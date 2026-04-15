/**
 * Test suite for krep directory search functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

/* Define TESTING before including headers if not done by Makefile */
#ifndef TESTING
#define TESTING
#endif

#include "../krep.h"

// Constants definitions
#define TEST_DIR_BASE "/tmp/krep_test_dir"
#define TEST_FILE_MAX_SIZE 8192

// Function declarations
static void create_test_directory_structure(void);
static void cleanup_test_directory_structure(void);
static void create_binary_file(const char *path);
static void create_text_file(const char *path, const char *content);
static void create_nested_directory(const char *base_path, int depth, int max_depth);

/**
 * Nested directory search test
 */
void test_recursive_directory_search(void)
{
    fprintf(stderr, "[test/test_directory.c] enter test_recursive_directory_search 1\n");
    printf("\n=== Testing Recursive Directory Search ===\n");

    // Setup: Create test directory structure
    create_test_directory_structure();
    // fprintf(stderr, "[test/test_directory.c] exit test_recursive_directory_search 1\n");

    // Configure basic search parameters
    fprintf(stderr, "[test/test_directory.c] enter test_recursive_directory_search 2\n");
    search_params_t params = {0};
    params.patterns = malloc(sizeof(char *));
    params.pattern_lens = malloc(sizeof(size_t));
    // fprintf(stderr, "[test/test_directory.c] exit test_recursive_directory_search 2\n");
    
    if (!params.patterns || !params.pattern_lens)
    {
        fprintf(stderr, "[test/test_directory.c] enter test_recursive_directory_search 3\n");
        fprintf(stderr, "Failed to allocate memory for test parameters\n");
        cleanup_test_directory_structure();
        return;
        // fprintf(stderr, "[test/test_directory.c] exit test_recursive_directory_search 3\n");
    }

    fprintf(stderr, "[test/test_directory.c] enter test_recursive_directory_search 4\n");
    // Pattern that exists only in some files
    const char *test_pattern = "FINDME";
    params.patterns[0] = (char *)test_pattern;
    params.pattern_lens[0] = strlen(test_pattern);
    params.num_patterns = 1;
    params.case_sensitive = true;
    params.use_regex = false;
    params.count_lines_mode = false;
    params.track_positions = true;
    params.max_count = SIZE_MAX;

    // Execute recursive search on the test directory
    printf("Testing recursive search for pattern '%s'...\n", test_pattern);

    // Reset global_match_found_flag
    extern atomic_bool global_match_found_flag;
    atomic_store(&global_match_found_flag, false);

    int errors = search_directory_recursive(TEST_DIR_BASE, &params, 1);

    // Verify results
    printf("Recursive search completed with %d errors\n", errors);
    bool matches_found = atomic_load(&global_match_found_flag);
    printf("Matches found: %s\n", matches_found ? "YES" : "NO");
    // fprintf(stderr, "[test/test_directory.c] exit test_recursive_directory_search 4\n");

    if (errors > 0)
    {
        fprintf(stderr, "[test/test_directory.c] enter test_recursive_directory_search 5\n");
        printf("FAIL: Recursive search reported errors\n");
        // fprintf(stderr, "[test/test_directory.c] exit test_recursive_directory_search 5\n");
    }
    else if (!matches_found)
    {
        fprintf(stderr, "[test/test_directory.c] enter test_recursive_directory_search 6\n");
        printf("FAIL: Recursive search didn't find expected matches\n");
        // fprintf(stderr, "[test/test_directory.c] exit test_recursive_directory_search 6\n");
    }
    else
    {
        fprintf(stderr, "[test/test_directory.c] enter test_recursive_directory_search 7\n");
        printf("PASS: Recursive search found matches without errors\n");
        // fprintf(stderr, "[test/test_directory.c] exit test_recursive_directory_search 7\n");
    }

    fprintf(stderr, "[test/test_directory.c] enter test_recursive_directory_search 8\n");
    // Cleanup
    free(params.patterns);
    free(params.pattern_lens);
    cleanup_test_directory_structure();
    // fprintf(stderr, "[test/test_directory.c] exit test_recursive_directory_search 8\n");
}

/**
 * Binary file handling test
 */
void test_binary_file_handling(void)
{
    fprintf(stderr, "[test/test_directory.c] enter test_binary_file_handling 1\n");
    printf("\n=== Testing Binary File Handling ===\n");

    // Create a test binary file
    const char *binary_file_path = "/tmp/krep_test_binary.bin";
    create_binary_file(binary_file_path);
    // fprintf(stderr, "[test/test_directory.c] exit test_binary_file_handling 1\n");

    // Configure search parameters
    fprintf(stderr, "[test/test_directory.c] enter test_binary_file_handling 2\n");
    search_params_t params = {0};
    params.patterns = malloc(sizeof(char *));
    params.pattern_lens = malloc(sizeof(size_t));
    // fprintf(stderr, "[test/test_directory.c] exit test_binary_file_handling 2\n");
    
    if (!params.patterns || !params.pattern_lens)
    {
        fprintf(stderr, "[test/test_directory.c] enter test_binary_file_handling 3\n");
        fprintf(stderr, "Failed to allocate memory for test parameters\n");
        unlink(binary_file_path);
        return;
        // fprintf(stderr, "[test/test_directory.c] exit test_binary_file_handling 3\n");
    }

    fprintf(stderr, "[test/test_directory.c] enter test_binary_file_handling 4\n");
    // Pattern that might exist in the binary file (we'll search for 'AB' which will be present)
    const char *test_pattern = "AB";
    params.patterns[0] = (char *)test_pattern;
    params.pattern_lens[0] = strlen(test_pattern);
    params.num_patterns = 1;
    params.case_sensitive = true;
    params.use_regex = false;
    params.count_lines_mode = false;
    params.track_positions = true;
    params.max_count = SIZE_MAX;

    // Execute search on the binary file
    printf("Testing search on binary file...\n");
    int result = search_file(&params, binary_file_path, 1);

    // Verify results (should identify the file as binary or handle it appropriately)
    printf("Binary file search resulted in code: %d\n", result);
    printf("Note: We expect proper handling, either by skipping as binary or by searching safely.\n");

    // Cleanup
    free(params.patterns);
    free(params.pattern_lens);
    unlink(binary_file_path);
    // fprintf(stderr, "[test/test_directory.c] exit test_binary_file_handling 4\n");
}

/**
 * Main function for test execution
 */
int main(void)
{
    fprintf(stderr, "[test/test_directory.c] enter main 1\n");
    printf("Starting Directory and Binary File Tests\n");
    // fprintf(stderr, "[test/test_directory.c] exit main 1\n");

    // Verify permissions to create test directories and files
    if (access("/tmp", W_OK) != 0)
    {
        fprintf(stderr, "[test/test_directory.c] enter main 2\n");
        fprintf(stderr, "Cannot write to /tmp, skipping tests\n");
        return 1;
        // fprintf(stderr, "[test/test_directory.c] exit main 2\n");
    }

    fprintf(stderr, "[test/test_directory.c] enter main 3\n");
    // Run tests
    test_recursive_directory_search();
    test_binary_file_handling();

    printf("\nAll directory and binary file tests completed\n");
    return 0;
    // fprintf(stderr, "[test/test_directory.c] exit main 3\n");
}

/* ========================================================================= */
/* Test support functions                                                    */
/* ========================================================================= */

/**
 * Creates a test directory structure
 */
static void create_test_directory_structure(void)
{
    fprintf(stderr, "[test/test_directory.c] enter create_test_directory_structure 1\n");
    printf("Creating test directory structure at %s\n", TEST_DIR_BASE);

    // Remove any existing directories
    cleanup_test_directory_structure();

    // Create base directory
    mkdir(TEST_DIR_BASE, 0755);

    // Create regular test files
    create_text_file(TEST_DIR_BASE "/file1.txt", "This is a text file\nIt has the pattern FINDME here\nAnd more text");
    create_text_file(TEST_DIR_BASE "/file2.txt", "This file doesn't have the pattern\nJust normal text");
    create_text_file(TEST_DIR_BASE "/file3.log", "Log file with FINDME pattern\nMultiple times FINDME");

    // Create symbolic link to test that it's handled correctly
    symlink(TEST_DIR_BASE "/file1.txt", TEST_DIR_BASE "/link_to_file1.txt");

    // Create directories to skip (should_skip_directory)
    mkdir(TEST_DIR_BASE "/.git", 0755);
    create_text_file(TEST_DIR_BASE "/.git/file_in_git.txt", "This has FINDME but should be skipped");

    mkdir(TEST_DIR_BASE "/node_modules", 0755);
    create_text_file(TEST_DIR_BASE "/node_modules/file_in_modules.txt", "This has FINDME but should be skipped");

    // Create nested directories
    create_nested_directory(TEST_DIR_BASE, 0, 3);

    // Create files with extensions to skip
    create_binary_file(TEST_DIR_BASE "/binary.exe");
    create_text_file(TEST_DIR_BASE "/minified.min.js", "function minified(){console.log('FINDME')}");
    create_text_file(TEST_DIR_BASE "/image.jpg", "Not a real image but should be skipped FINDME");

    printf("Test directory structure created\n");
    // fprintf(stderr, "[test/test_directory.c] exit create_test_directory_structure 1\n");
}

/**
 * Cleans up the test directory structure
 */
static void cleanup_test_directory_structure(void)
{
    fprintf(stderr, "[test/test_directory.c] enter cleanup_test_directory_structure 1\n");
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", TEST_DIR_BASE);
    system(cmd);
    // fprintf(stderr, "[test/test_directory.c] exit cleanup_test_directory_structure 1\n");
}

/**
 * Creates a test binary file
 */
static void create_binary_file(const char *path)
{
    fprintf(stderr, "[test/test_directory.c] enter create_binary_file 1\n");
    FILE *f = fopen(path, "wb");
    // fprintf(stderr, "[test/test_directory.c] exit create_binary_file 1\n");
    
    if (!f)
    {
        fprintf(stderr, "[test/test_directory.c] enter create_binary_file 2\n");
        fprintf(stderr, "Failed to create binary file: %s\n", path);
        return;
        // fprintf(stderr, "[test/test_directory.c] exit create_binary_file 2\n");
    }

    fprintf(stderr, "[test/test_directory.c] enter create_binary_file 3\n");
    // Create binary data with NULL bytes and recognizable patterns
    unsigned char data[TEST_FILE_MAX_SIZE];
    for (int i = 0; i < TEST_FILE_MAX_SIZE; i++)
    {
        if (i % 32 == 0)
        {
            fprintf(stderr, "[test/test_directory.c] enter create_binary_file 4\n");
            data[i] = 0; // NULL byte every 32 bytes
            // fprintf(stderr, "[test/test_directory.c] exit create_binary_file 4\n");
        }
        else if (i % 128 < 2)
        {
            fprintf(stderr, "[test/test_directory.c] enter create_binary_file 5\n");
            data[i] = 'A' + (i % 2); // Insert "AB" every 128 bytes
            // fprintf(stderr, "[test/test_directory.c] exit create_binary_file 5\n");
        }
        else
        {
            fprintf(stderr, "[test/test_directory.c] enter create_binary_file 6\n");
            data[i] = (i % 95) + 32; // Printable ASCII characters
            // fprintf(stderr, "[test/test_directory.c] exit create_binary_file 6\n");
        }
    }

    fwrite(data, 1, TEST_FILE_MAX_SIZE, f);
    fclose(f);
    // fprintf(stderr, "[test/test_directory.c] exit create_binary_file 3\n");
}

/**
 * Creates a text file with specific content
 */
static void create_text_file(const char *path, const char *content)
{
    fprintf(stderr, "[test/test_directory.c] enter create_text_file 1\n");
    FILE *f = fopen(path, "w");
    // fprintf(stderr, "[test/test_directory.c] exit create_text_file 1\n");
    
    if (!f)
    {
        fprintf(stderr, "[test/test_directory.c] enter create_text_file 2\n");
        fprintf(stderr, "Failed to create text file: %s\n", path);
        return;
        // fprintf(stderr, "[test/test_directory.c] exit create_text_file 2\n");
    }

    fprintf(stderr, "[test/test_directory.c] enter create_text_file 3\n");
    fputs(content, f);
    fclose(f);
    // fprintf(stderr, "[test/test_directory.c] exit create_text_file 3\n");
}

/**
 * Creates nested directories with test files
 */
static void create_nested_directory(const char *base_path, int depth, int max_depth)
{
    fprintf(stderr, "[test/test_directory.c] enter create_nested_directory 1\n");
    if (depth >= max_depth)
        return;
    // fprintf(stderr, "[test/test_directory.c] exit create_nested_directory 1\n");

    // Create 2 subdirectories for each level
    for (int i = 1; i <= 2; i++)
    {
        fprintf(stderr, "[test/test_directory.c] enter create_nested_directory 2\n");
        char subdir_path[PATH_MAX];
        snprintf(subdir_path, sizeof(subdir_path), "%s/level%d_%d", base_path, depth, i);

        mkdir(subdir_path, 0755);

        // Create files in this directory
        char file_path[PATH_MAX];
        snprintf(file_path, sizeof(file_path), "%s/file_level%d_%d.txt", subdir_path, depth, i);
        // fprintf(stderr, "[test/test_directory.c] exit create_nested_directory 2\n");

        // Put the pattern only in files at even levels
        if (depth % 2 == 0)
        {
            fprintf(stderr, "[test/test_directory.c] enter create_nested_directory 3\n");
            create_text_file(file_path, "This file has FINDME pattern in nested directory");
            // fprintf(stderr, "[test/test_directory.c] exit create_nested_directory 3\n");
        }
        else
        {
            fprintf(stderr, "[test/test_directory.c] enter create_nested_directory 4\n");
            create_text_file(file_path, "This file doesn't have the pattern");
            // fprintf(stderr, "[test/test_directory.c] exit create_nested_directory 4\n");
        }

        fprintf(stderr, "[test/test_directory.c] enter create_nested_directory 5\n");
        // Recursion
        create_nested_directory(subdir_path, depth + 1, max_depth);
        // fprintf(stderr, "[test/test_directory.c] exit create_nested_directory 5\n");
    }
}
// Total cost: 0.094326
// Total split cost: 0.000000, input tokens: 0, output tokens: 0, cache read tokens: 0, cache write tokens: 0, split chunks: [(0, 304)]
// Total instrumented cost: 0.094326, input tokens: 5192, output tokens: 3953, cache read tokens: 0, cache write tokens: 5188
