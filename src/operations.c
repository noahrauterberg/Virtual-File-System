#include "../lib/operations.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static char root_name = '/';

/*
 * returns index of smallest free direct_block or -1 when there is no free block left
 */
int find_free_direct_block(inode* node) {
    for (int i=0; i<DIRECT_BLOCKS_COUNT; i++) {
        if (node->direct_blocks[i] == -1) {return i;}
    }
    return -1;
}

/*
 * returns index of next free data_block or -1 if there is no free block left
 */
int find_free_data_block(file_system* fs) {
    for (int i=0; i < fs->s_block->num_blocks; i++) {
        if (fs->free_list[i] == 1) {return i;}
    }
    return -1;
}

/*
 * returns index of sub node with correct name and filetype or -1 if such a node does not exist
 */
int find_name_on_node(file_system* fs, inode* cur_node, char* name, int file_type) {
    for (int i=0; i<DIRECT_BLOCKS_COUNT; i++) {
        int potential_index = cur_node->direct_blocks[i];
        if (potential_index == -1) {continue;}
        inode* potential_node = &fs->inodes[potential_index];
        if ((!strcmp(potential_node->name, name)) && (potential_node->n_type == file_type)) {return potential_index;}
    }
    return -1;
}

/*
 * returns index of the inode referenced by path or -1 if the path is invalid
 */
int find_node_of_path(file_system* fs, char* path, int file_type) {
    if (!(strcmp(path, "/")))  {
        if (file_type == directory) {
            return fs->root_node;
        } else {
            return  -1;
        }
    }

    // if path doesn't start with root name, this is invalid
    if (path[0] != '/') {return -1;}
    // removing root name from path for further use
    path++;

    int cur_index = fs->root_node;
    inode* cur_node = &fs->inodes[cur_index];
    // traversing through directories
    int found;
    char* next_delim = strchr(path, root_name);
    while (next_delim != NULL) {
        // what is the name of the dir we are searching for
        char* next_dir_name = calloc(next_delim-path, sizeof(char));
        strncpy(next_dir_name, path, next_delim - path);

        found = find_name_on_node(fs, cur_node, next_dir_name, directory);
        free(next_dir_name);
        if (found == -1) {return -1;}
        cur_node = &fs->inodes[found];
        // adjusting the remaining path to start after next_delim
        path = next_delim + 1;
        next_delim = strchr(path, root_name);
    }

    // reached last node on our path
    found = find_name_on_node(fs, cur_node, path, file_type);
    if (found == -1) {return -1;}
    cur_index = found;
    
    return cur_index;
}

int create_inode_on_path(file_system* fs, char* path, int file_type) {
    if ((path == NULL) || (fs == NULL)) {return -1;}
    // searching for lowest index to store new inode
    int new_index = find_free_inode(fs);
    
    // there is no free inode left
    if (new_index == -1) {return -1;}
    inode* new_node = &fs->inodes[new_index];

    int root_index = fs->root_node;
    // is path more than just the root dir?
    if (!(strcmp(path, "/"))) {
        if ((root_index <= DIRECT_BLOCKS_COUNT) && (root_index > -1) && (fs->inodes[root_index].n_type != free_block)) {
            // root has already been created
            return -2;
        }
        // this should never happen -> why would we create the root_dir manually?
        // should never be the case due to fs_create()
        fs->root_node = new_index;
        new_node->parent = -1;
        new_node->n_type = directory;
        return 0;
    }

    char* new_name = strrchr(path, root_name);
    if (new_name == NULL) {return -1;}

    // adjusting path to end before name of new inode
    *new_name = '\0';
    // removing '/' from new_name
    new_name++;

    inode* parent_node;
    int parent_index;
    if (strlen(path) > 0) {
        parent_index = find_node_of_path(fs, path, directory);
        if (parent_index == -1) {return -1;}
        parent_node = &fs->inodes[parent_index];
    } else {
        parent_node = &fs->inodes[root_index];
        parent_index = root_index;
    }

    if (find_name_on_node(fs, parent_node, new_name, file_type) != -1) {return -2;}

    int direct_index = find_free_direct_block(parent_node);
    if (direct_index == -1) {return -1;}
    parent_node->direct_blocks[direct_index] = new_index;
    new_node->parent = parent_index;
    new_node->n_type = file_type;
    strncpy(new_node->name, new_name, NAME_MAX_LENGTH);
    // resetting everything
    memset(new_node->direct_blocks, -1, DIRECT_BLOCKS_COUNT * sizeof(int));

    return 0;
}

/*
 * returns how many characters of text have been written
 */
size_t write_block_on_index(data_block* db, int index, char* text) {
    size_t write;
    if (strlen(text) <= BLOCK_SIZE - index) {
        write = strlen(text);
    } else {
        write = BLOCK_SIZE - index;
    }
    memcpy(db->block + index, text, write);
    db->size += write;
    return write;
}

char* read_file_node(file_system* fs, inode* cur_node, int* file_size) {
    char* db_content[DIRECT_BLOCKS_COUNT] = {NULL};

    *file_size = 0;
    int used_db = 0;
    for (int i = 0; i < DIRECT_BLOCKS_COUNT; i++) {
        int db_index = cur_node->direct_blocks[i];
        if (db_index == -1) {continue;}
        data_block* cur_block = &fs->data_blocks[db_index];

        char* cur_content = calloc(cur_block->size, sizeof(char));
        if (cur_content == NULL) {return NULL;}
        memcpy(cur_content, cur_block->block, cur_block->size);
        db_content[used_db] = cur_content;
        *file_size += cur_block->size;
        used_db++;
    }

    if (used_db == 0) {return NULL;}

    char* output = calloc(*file_size, sizeof(char));
    if (output == NULL) {return NULL;}
    strcpy(output, "");
    for (int i=0; i<used_db; i++) {
        strcat(output, db_content[i]);
        free(db_content[i]);
    }
    return output;
}

int fs_mkdir(file_system* fs, char* path) {
    int retval = create_inode_on_path(fs, path, directory);
    if (retval < 0) {return -1;}

    return 0;
}

int fs_mkfile(file_system *fs, char *path_and_name) {
    int retval = create_inode_on_path(fs, path_and_name, reg_file);

    return retval;
}

char* fs_list(file_system *fs, char *path) {
    if (path == NULL) {
        return NULL;
    }
    // shouldn't be necessary
    if (fs == NULL) {return NULL;}

    int dir_index = find_node_of_path(fs, path, directory);
    if (dir_index == -1) {return NULL;}
    inode* listing_dir = &fs->inodes[dir_index];
    if (listing_dir == NULL) {return NULL;}

    int used_blocks[DIRECT_BLOCKS_COUNT] = {0};
    int next_index = 0;
    // getting inode indices of used direct_blocks
    for (int i=0; i<DIRECT_BLOCKS_COUNT; i++) {
        int next_inode_index = listing_dir->direct_blocks[i];
        if (next_inode_index == -1) { continue; }


        // inserting indices sorted into array ---

        // searching for insert spot of next_inode_index
        int cur_insert = next_index;
        while ((cur_insert > 0) && (used_blocks[cur_insert-1] > next_inode_index)) {
            cur_insert--;
        }

        // shifting used_blocks to insert new value
        for (int j=next_index; j>cur_insert; j--) {
            used_blocks[j] = used_blocks[j-1];
        }

        used_blocks[cur_insert] = next_inode_index;
        next_index++;
    }

    // directory is empty
    if (next_index == 0) {
        char* empty_string = malloc(strlen("") + 1);
        strcpy(empty_string, "");
        return empty_string;
    }


    char* lines_arr[next_index - 1];
    size_t result_size = 0;

    char* fil = "FIL ";
    char* dir = "DIR ";
    char* line_break = "\n";
    for (int i=0; i<next_index; i++) {
        inode* cur_node = &fs->inodes[used_blocks[i]];
        char* node_name = cur_node->name;
        char* cur_line = malloc(strlen(fil) + strlen(node_name) + strlen(line_break) + 1);

        if (cur_node->n_type == directory) {
            strcpy(cur_line, dir);
        } else if (cur_node->n_type == reg_file) {
            strcpy(cur_line, fil);
        }
        strcat(cur_line, node_name);
        strcat(cur_line, line_break);
        lines_arr[i] = cur_line;
        result_size += strlen(cur_line);
    }

    char* result = malloc(result_size + 1);
    if (result == NULL) {return NULL;}
    strcpy(result, "");
    for (int i=0; i<next_index; i++) {
        strcat(result, lines_arr[i]);
        free(lines_arr[i]);
    }

    return result;
}

int fs_writef(file_system *fs, char *filename, char *text) {
    int file_index = find_node_of_path(fs, filename, reg_file);
    inode* file_node = &fs->inodes[file_index];

    // path is invalid or doesn't point to a file
    if (file_node == NULL || file_node->n_type != reg_file) {return -1;}

    // is this file empty?->free = index of first free direct_block
    int free = -1;
    for (int i=0; i<DIRECT_BLOCKS_COUNT; i++) {
        if (file_node->direct_blocks[i] == -1) {
            free = i;
            break;
        }
    }

    if (free == -1) {return -2;}

    // free == 0->empty file->start to write
    // free > 0->file not empty->how much space in free-1?->write

    int count = 0;
    if (free > 0) {
        data_block* cur_block = &fs->data_blocks[file_node->direct_blocks[free - 1]];
        if (cur_block->size < BLOCK_SIZE) {
            // we can still write in that block->writing 'write' many characters in that block
            int write = write_block_on_index(cur_block, cur_block->size, text);
            text += write;
            count += write;
        }
    }

    // writing to blocks
    while (strlen(text) > 0) {
        int cur_db_index = find_free_data_block(fs);
        file_node->direct_blocks[free] = cur_db_index;
        data_block* cur_block = &fs->data_blocks[cur_db_index];

        int write = write_block_on_index(cur_block, 0, text);
        file_node->size += write;
        fs->free_list[cur_db_index] = 0;
        text += write;
        free = find_free_direct_block(file_node);
        count += write;
        fs->s_block->free_blocks--;
    }

	return count;
}

uint8_t* fs_readf(file_system *fs, char *filename, int *file_size) {
    int file_index = find_node_of_path(fs, filename, reg_file);
    if (file_index == -1) {return NULL;}
    inode* file_node = &fs->inodes[file_index];

    char* output = read_file_node(fs, file_node, file_size);
    // necessary ?
    if (output == NULL) {return NULL;}

    if (strlen(output) == 0) {return NULL;}

    return (uint8_t*)output;
}

int rm_file(file_system* fs, inode* cur_node) {
    for (int i=0; i<DIRECT_BLOCKS_COUNT; i++) {
        int next_index = cur_node->direct_blocks[i];
        if (next_index == -1 ) {continue;}
        data_block* next_block = &fs->data_blocks[next_index];
        next_block->size = 0;
        fs->free_list[next_index] = 1;
        fs->s_block->free_blocks++;
    }
    cur_node->n_type = free_block;
    return 0;
}

int rm_dir(file_system* fs, inode* cur_node) {
    int retval = 0;
    for (int i=0; i<DIRECT_BLOCKS_COUNT; i++) {
        int next_index = cur_node->direct_blocks[i];
        if (next_index == -1) {continue;}
        inode* next_node = &fs->inodes[next_index];
        if (next_node->n_type == directory) {retval = rm_dir(fs, next_node);}
        else if (next_node->n_type == reg_file) {retval = rm_file(fs, next_node);}

        if (retval == -1) {break;}
        cur_node->direct_blocks[i] = -1;
    }
    cur_node->n_type = free_block;
    return retval;
}

/*
 * removes dir if multiple nodes have the same name
 */
int fs_rm(file_system *fs, char *path) {
    int file_type = directory;
    int node_index = find_node_of_path(fs, path, directory);
    if (node_index == -1) {
        node_index = find_node_of_path(fs, path, reg_file);
        file_type = reg_file;
        if (node_index == -1) {return -1;}
    }
    inode* cur_node = &fs->inodes[node_index];
    inode* parent_node = &fs->inodes[cur_node->parent];

    for (int i=0; i<DIRECT_BLOCKS_COUNT; i++) {
        int pot_index = parent_node->direct_blocks[i];
        if (pot_index != node_index) {continue;}
        parent_node->direct_blocks[i] = -1;
    }

    if (file_type == directory) {return rm_dir(fs, cur_node);}
    else {return rm_file(fs, cur_node);}
}

int fs_import(file_system* fs, char* int_path, char* ext_path) {
    FILE* import = fopen(ext_path, "r");
    if (import == NULL) {return -1;}

    int max_size = DIRECT_BLOCKS_COUNT * sizeof(char) * BLOCK_SIZE;
    char* input = malloc(max_size + 1);
    input[max_size+1] = '\0';
    if (input == NULL) {return -1;}

    size_t bytes_read = fread(input, sizeof(char), max_size, import);

    // is the file larger than what we can store?
    if (bytes_read == max_size) {
        char* input2 = malloc(sizeof(char) + 1);
        if (input2 == NULL) {return -1;}
        size_t bytes_read2 = fread(input2, sizeof(char), 1, import);
        if (bytes_read2 > 0) {
            free(input2);
            free(input);
            return -1;
        }
    }

    input[bytes_read] = '\0';

    int retval = fs_writef(fs, int_path, input);
    free(input);

    fclose(import);

    if (retval >= 0) {return 0;}
    return -1;
}

int fs_export(file_system *fs, char *int_path, char *ext_path) {
    int node_index = find_node_of_path(fs, int_path, reg_file);
    if (node_index == -1) {return -1;}
    inode* node = &fs->inodes[node_index];

    FILE* export = fopen(ext_path, "a");
    if (export == NULL) {return -1;}

    int size = 0;
    char* output = read_file_node(fs, node, &size);
    if (output == NULL) {return -1;}

    int wrote = fwrite(output, sizeof(char), strlen(output), export);

    fclose(export);

    free(output);
    if (wrote != size) {return -1;}

	return 0;
}
