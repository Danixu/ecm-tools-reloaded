#include <iostream>
#include <fstream>
#include <ctime>
#include <math.h>

#define getTime() double(clock())/CLOCKS_PER_SEC
#define MB (1024*1024)
// power of 2: 5 means 2^5
#define max_block_size 11

int main(){
    double start;
    uint32_t size = 20 * MB;

    // dummy block used to write data
    char block[(uint32_t)pow(2, max_block_size) + 1];
    // Filling dummy block with random data
    for (uint32_t i = 0; i < (uint32_t)pow(2, max_block_size); i++) {
        block[i] = rand() % 255;
    }

    //
    // Sequencial Write
    //
    // write C buffersize 2^1 to 2^12
    for (uint8_t i = 0; i <= max_block_size; i++) {
        for (uint8_t k = 10; k < 20; k++) {
            FILE* file2 = fopen("test.txt", "wb");

            char buffer[(uint32_t)pow(2, k)];
            uint32_t blocksize = (uint32_t)pow(2, i);
            setvbuf(file2, buffer, _IOFBF, (uint32_t)pow(2, k));

            start = getTime();
            //for(int l = 0; l < (size); l++) file << "a";
            for(int l = 0; l < (size / blocksize); l++) fwrite(block, blocksize, 1, file2);
            std::cout << "Secuencial WriteC with blocksize " << pow(2, i) << " & buffer " << pow(2, k) << ": " << getTime() - start << '\n';

            fclose(file2);
        }
    }
    // write CPP buffersize 2^1 to 2^12
    for (uint8_t i = 0; i <= max_block_size; i++) {
        for (uint8_t k = 10; k < 20; k++) {
            std::ofstream file;
            file.open("test.txt", std::ios::trunc|std::ios::binary);

            char buffer[(uint32_t)pow(2, k)];
            uint32_t blocksize = (uint32_t)pow(2, i);
            file.rdbuf()->pubsetbuf(buffer, pow(2, k));

            start = getTime();
            //for(int l = 0; l < (size); l++) file << "a";
            for(int l = 0; l < (size / blocksize); l++) file.write(block, blocksize);
            std::cout << "Secuencial WriteCPP with blocksize " << pow(2, i) << " & buffer " << pow(2, k) << ": " << getTime() - start << '\n';

            file.close();
        }
    }


    //
    // Random Write
    //
    // write C buffersize 2^1 to 2^12
    for (uint8_t i = 0; i <= max_block_size; i++) {
        for (uint8_t k = 10; k < 20; k++) {
            FILE* file2 = fopen("test.txt", "wb");

            uint32_t blocksize = (uint32_t)pow(2, i);
            uint32_t blocks_to_write = (size / blocksize);
            uint32_t *positions; positions =  new uint32_t[blocks_to_write];
            for (uint32_t z = 0; z < blocks_to_write; z++) {
                positions[z] = blocksize * (rand() % blocks_to_write);
            }

            char buffer[(uint32_t)pow(2, k)];
            setvbuf(file2, buffer, _IOFBF, (uint32_t)pow(2, k));

            start = getTime();
            //for(int l = 0; l < (size); l++) file << "a";
            for(int l = 0; l < blocks_to_write; l++) {
                fseek(file2, positions[l], SEEK_SET);
                fwrite(block, blocksize, 1, file2);
            }
            std::cout << "Random WriteC with blocksize " << pow(2, i) << " & buffer " << pow(2, k) << ": " << getTime() - start << '\n';

            fclose(file2);
        }
    }
    // write CPP buffersize 2^1 to 2^12
    for (uint8_t i = 0; i <= max_block_size; i++) {
        for (uint8_t k = 10; k < 20; k++) {
            std::ofstream file;
            file.open("test.txt", std::ios::trunc|std::ios::binary);

            uint32_t blocksize = (uint32_t)pow(2, i);
            uint32_t blocks_to_write = (size / blocksize);
            uint32_t *positions; positions =  new uint32_t[blocks_to_write];
            for (uint32_t z = 0; z < blocks_to_write; z++) {
                positions[z] = blocksize * (rand() % blocks_to_write);
            }

            char buffer[(uint32_t)pow(2, k)];
            file.rdbuf()->pubsetbuf(buffer, pow(2, k));

            start = getTime();
            //for(int l = 0; l < (size); l++) file << "a";
            for(int l = 0; l < (size / blocksize); l++) {
                file.seekp(positions[l]);
                file.write(block, blocksize);
            }
            std::cout << "Random WriteCPP with blocksize " << pow(2, i) << " & buffer " << pow(2, k) << ": " << getTime() - start << '\n';

            file.close();
        }
    }

    // Generate a dummy file to be readed
    FILE* file2 = fopen("test.txt", "wb");
    for (uint32_t i = 0; i < (size / max_block_size); i++) {
        fwrite(block, max_block_size, 1, file2);
    }
    fclose(file2);

    //
    // Sequencial Read
    //
    // read C buffersize 2^1 to 2^12
    for (uint8_t i = 0; i <= max_block_size; i++) {
        for (uint8_t k = 10; k < 20; k++) {
            FILE* file2 = fopen("test.txt", "rb");

            char buffer[(uint32_t)pow(2, k)];
            uint32_t blocksize = (uint32_t)pow(2, i);
            setvbuf(file2, buffer, _IOFBF, (uint32_t)pow(2, k));

            start = getTime();
            //for(int l = 0; l < (size); l++) file << "a";
            for(int l = 0; l < (size / blocksize); l++) fread(block, blocksize, 1, file2);
            std::cout << "Secuencial ReadC with blocksize " << pow(2, i) << " & buffer " << pow(2, k) << ": " << getTime() - start << '\n';

            fclose(file2);
        }
    }
    // read CPP buffersize 2^1 to 2^12
    for (uint8_t i = 0; i <= max_block_size; i++) {
        for (uint8_t k = 10; k < 20; k++) {
            std::ifstream file;
            file.open("test.txt", std::ios::binary);

            char buffer[(uint32_t)pow(2, k)];
            uint32_t blocksize = (uint32_t)pow(2, i);
            file.rdbuf()->pubsetbuf(buffer, pow(2, k));

            start = getTime();
            //for(int l = 0; l < (size); l++) file << "a";
            for(int l = 0; l < (size / blocksize); l++) file.read(block, blocksize);
            std::cout << "Secuencial ReadCPP with blocksize " << pow(2, i) << " & buffer " << pow(2, k) << ": " << getTime() - start << '\n';

            file.close();
        }
    }

    //
    // Random Read
    //
    // read C buffersize 2^1 to 2^12
    for (uint8_t i = 0; i <= max_block_size; i++) {
        for (uint8_t k = 10; k < 20; k++) {
            FILE* file2 = fopen("test.txt", "rb");

            uint32_t blocksize = (uint32_t)pow(2, i);
            uint32_t blocks_to_read = (size / blocksize);
            uint32_t *positions; positions =  new uint32_t[blocks_to_read];
            for (uint32_t z = 0; z < blocks_to_read; z++) {
                positions[z] = blocksize * (rand() % blocks_to_read);
            }

            char buffer[(uint32_t)pow(2, k)];
            setvbuf(file2, buffer, _IOFBF, (uint32_t)pow(2, k));

            start = getTime();
            //for(int l = 0; l < (size); l++) file << "a";
            for(int l = 0; l < blocks_to_read; l++) {
                fseek(file2, positions[l], SEEK_SET);
                fread(block, blocksize, 1, file2);
            }
            std::cout << "Random ReadC with blocksize " << pow(2, i) << " & buffer " << pow(2, k) << ": " << getTime() - start << '\n';

            fclose(file2);
        }
    }
    // read CPP buffersize 2^1 to 2^12
    for (uint8_t i = 0; i <= max_block_size; i++) {
        for (uint8_t k = 10; k < 20; k++) {
            std::ifstream file;
            file.open("test.txt", std::ios::binary);

            uint32_t blocksize = (uint32_t)pow(2, i);
            uint32_t blocks_to_read = (size / blocksize);
            uint32_t *positions; positions =  new uint32_t[blocks_to_read];
            for (uint32_t z = 0; z < blocks_to_read; z++) {
                positions[z] = blocksize * (rand() % blocks_to_read);
            }

            char buffer[(uint32_t)pow(2, k)];
            file.rdbuf()->pubsetbuf(buffer, pow(2, k));

            start = getTime();
            //for(int l = 0; l < (size); l++) file << "a";
            for(int l = 0; l < (size / blocksize); l++) {
                file.seekg(positions[l]);
                file.read(block, blocksize);
            }
            std::cout << "Random ReadCPP with blocksize " << pow(2, i) << " & buffer " << pow(2, k) << ": " << getTime() - start << '\n';

            file.close();
        }
    }
}