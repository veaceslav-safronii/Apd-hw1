#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <pthread.h>
#include <queue>
#include <fstream>
#include <iostream>
#include <set>
#include <vector>
#include <algorithm>
#include <unordered_map>

using namespace std;

enum ThreadType {
    MAPPER,
    REDUCER
};

// Shared data
struct ThreadData {
    int num_mappers;
    int num_reducers;
    queue<pair<string, int>> *input_files;
    pthread_mutex_t *mutex;
    pthread_barrier_t *barrier;
    unordered_map<string, set<int>> *mapper_results;
};

// Individual thread arguments
struct ThreadArg {
    int id;
    ThreadType type;
    ThreadData *data;
};

void *mapper(void *arg) {
    ThreadArg *thread_arg = (ThreadArg *)arg;
    ThreadData *data = thread_arg->data;
    unordered_map<string, set<int>> local_result;

    while (true) {
        string file_name;
        int file_id;

        pthread_mutex_lock(data->mutex);
        // Checking if there are any files left to process
        if (data->input_files->empty()) {
            pthread_mutex_unlock(data->mutex);
            break;
        }
        // Getting the next file to process
        auto file = data->input_files->front();
        data->input_files->pop();
        pthread_mutex_unlock(data->mutex);

        file_name = file.first;
        file_id = file.second;

        ifstream f(file_name);
        if (f.fail()) {
            cerr << "Failed to open file " << file_name << endl;
            continue;
        }
        // Processing the file
        while (!f.eof()) {
            string word;
            f >> word;
    
            // Removing non-alphabetic characters and converting to lowercase
            transform(word.begin(), word.end(), word.begin(), ::tolower);
            word.erase(remove_if(word.begin(), word.end(),
                [](char c) { return !isalpha(c); }), word.end()
            );

            if (word.empty()) {
                continue;
            }
            // Adding result to the local data structure
            local_result[word].insert(file_id);
        }

        f.close();
    }
    // Adding the results to the shared data structure
    pthread_mutex_lock(data->mutex);
    for (const auto &entry : local_result) {
        (*data->mapper_results)[entry.first].insert(
            entry.second.begin(),
            entry.second.end()
        );
    }
    pthread_mutex_unlock(data->mutex);

    pthread_barrier_wait(data->barrier);

    return NULL;
}

void *reducer(void *arg) {
    ThreadArg *thread_arg = (ThreadArg *)arg;
    ThreadData *data = thread_arg->data;

    // Wait for all the mappers to finish
    pthread_barrier_wait(data->barrier);

    int reducer_id = thread_arg->id - data->num_mappers;

    // Spliting the letters between the reducers
    int start = reducer_id  * 26 / data->num_reducers;
    int end = min((reducer_id + 1) * 26 / data->num_reducers, 26);

    for (char letter = 'a' + start; letter < 'a' + end; ++letter) {
        vector<pair<string, set<int>>> words;
        // Get all the words that start with the current letter
        for (const auto &entry : *data->mapper_results) {
            if (entry.first[0] == letter) {
                words.push_back({entry.first, entry.second});
            }
        }

        // Sorting the words by the number of files they appear in
        sort(words.begin(), words.end(),
            [](const pair<string, set<int>> &a,
                const pair<string, set<int>> &b) {
            if (a.second.size() == b.second.size()) {
                return a.first < b.first;
            }
            return a.second.size() > b.second.size();
        });

        // Creating the output file
        string filename(1, letter);
        filename += ".txt";
        ofstream outfile(filename);
        // Writing the words to the file
        for (const auto &word_entry : words) {
            const string &word = word_entry.first;
            const set<int> &file_ids = word_entry.second;
            outfile << word << ":[";
            for (auto it = file_ids.begin(); it != file_ids.end(); ++it) {
                if (it != file_ids.begin()) {
                    outfile << " ";
                }
                outfile << *it;
            }
            outfile << "]\n";
        }

        outfile.close();

    }

    return NULL;
}

void *thread_function(void *arg) {
    ThreadArg *thread_arg = (ThreadArg *)arg;
    // Calling the appropriate function based on the thread type
    if (thread_arg->type == MAPPER) {
        mapper(arg);
    } else {
        reducer(arg);
    }

    pthread_exit(NULL);
}

int main(int argc, char **argv) {
    if (argc < 4) {
        cerr << "Usage: " << argv[0] << " <num_mappers> <num_reducers> <input_file>\n";
        return 1;
    }

    int num_mappers = atoi(argv[1]);
    int num_reducers = atoi(argv[2]);
    char *input_file = argv[3];

    ifstream f(input_file);
    if (f.fail()) {
        cerr << "Failed to open input file\n";
        return 1;
    }

    int num_files;
    f >> num_files;

    queue<pair<string, int>> files;
    // Reading the input files and adding them to the queue with an id
    for (int i = 1; i <= num_files; i++) {
        string file_name;
        f >> file_name;
        files.push(make_pair(file_name, i));
    }

    pthread_t threads[num_mappers + num_reducers];

    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);

    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, num_mappers + num_reducers);

    unordered_map<string, set<int>> mapper_results;

    // Initializing the shared data structure
    ThreadData data = {num_mappers, num_reducers, &files, &mutex, &barrier,
        &mapper_results};

    // Individal thread data
    ThreadArg thread_args[num_mappers + num_reducers];

    // Creating the threads with the appropriate arguments and type
    for (int i = 0; i < num_mappers + num_reducers; i++) {
        thread_args[i].id = i;
        thread_args[i].data = &data;
        if (i < num_mappers) {
            thread_args[i].type = MAPPER;
        } else {
            thread_args[i].type = REDUCER;
        }
        pthread_create(&threads[i], NULL, thread_function, &thread_args[i]);
    }

    for (int i = 0; i < num_mappers + num_reducers; i++) {
        pthread_join(threads[i], NULL);
    }

    // Destroying the mutex and barrier
    pthread_mutex_destroy(&mutex);
    pthread_barrier_destroy(&barrier);

    return 0;
}