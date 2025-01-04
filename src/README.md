Safronii Veaceslav - 334CD

# Homework 1: `Inversed index calculation using Map-Reduce paradigm`

* For initialising threads, is used a structure that contains necessary fields for storing input and results of the mapper and reducer thread

* First num_mappers threads are assigned as mappers and the rest as reducers.

* Calling the corresponding function (`mapper`/ `reducer`) is done in `thread_function`.

* Initially, the program reads the input file and inserts the files from it with its id to a `queue` for `dynamic division of work` for mappers.

## Mapper

1. Every mapper pops a file from the queue.

2. Reads all words and removes the non-alphabetical symbols.

3. Inserts the word with the id of the file in a local `unordered map`.

4. After the `queue` is empty, all mappers insert its map values in the `shared unordered map`.

* At the end all threads are waiting a `barrier`.

## Reducer

1. At the start all reducers wait the `barrier` to unlock, so all the mappers finish their work.

2. Each thread will split the letters which they will process, obtaining the start and end letter, depending on the id of the thread. 

3. The `mapper_results` map is traversed by each reducer:
    - Saving, only, the words that start with the corresponding letter, in a local `vector of pairs` of a word and a `set` of file ids.

    - The words in that vector are sorted by the number of file ids, or in alphabetical order.

    - Then is created a file for that letter and there are written the words that start with that letter.





