#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/random.h>
#include <sys/wait.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <time.h>

// The file which will contain the initial message
#define INPUT_FILE "input_file"
// The file which will contain the decrypted message
#define OUTPUT_FILE "output_file"
// What will be appended to match the requiered length for messages
#define APPEND_CHAR '-'

class KPrime final {
private:
    char * buffer = nullptr;
    static KPrime * singleton;
    KPrime() {}
public:
    static KPrime* getSingleton()
    {
        if (!singleton)
            singleton = new KPrime();
        return singleton;
    }

    char* get() {
        if (buffer == nullptr)
        {
            buffer = new char[129];
            std::ifstream fin("k_prime_key", std::ios::binary);
            fin.read(buffer, 128);
        }
        return buffer;
    }
};

KPrime* KPrime::singleton = nullptr;

class KeyManager {
    
public:
    // Generate & encrypt a random key K
    std::string& getK()
    {
        // Generate a random key
        std::string k;
        k.resize(16);
        char * kBuffer = new char[16];
        getentropy(kBuffer, 16);
        k = kBuffer;
        delete kBuffer;

        // Write it to file "k_key"
        std::ofstream fout("k_key", std::ios::binary);
        fout.write(k.c_str(), 16);
        fout.close();

        // Encrypt the file "k_key"
        system("openssl enc -aes-128-cbc -e -in k_key -out enc_k_key -kfile k_prime_key");

        // Delete file
        system("rm -rf k_key");

        // Read the key from file
        std::ifstream fin("enc_k_key", std::ios::binary);

        auto start = fin.tellg();
        fin.seekg(0, std::ios::end);
        auto finish = fin.tellg();
        fin.seekg(0, std::ios::beg);

        char * encK = new char[finish - start];
        fin.read(encK, finish - start);
        fin.close();

        // Delete file
        system("rm -rf enc_k_key");

        // Test
        // system("openssl enc -aes-128-cbc -d -in enc_k_key -out TESTFILE -kfile k_prime_key");
        // merge, se decripteaza fara probleme

        std::string * result = new std::string();
        result->resize(finish - start);
        for (int i = 0; i < finish - start; i++)
            (*result)[i] = encK[i];
        return *result;
    }
};

int getFileSize(std::ifstream& f)
{
    auto start = f.tellg();
    f.seekg(0, std::ios::end);
    auto finish = f.tellg();
    f.seekg(0, std::ios::beg);
    return finish - start;
}

std::string encryptMessage(std::string& plaintext)
{
    // Generate a random file name
    std::string filename = "encrypt_";
    char buffer;
    for (int i = 0; i < 3; i++)
    {
        buffer = rand() % ('Z' - 'A') + 'A';
        filename += buffer;
    }
    std::ofstream fout(filename, std::ios::binary);

    // Write the data to the file
    fout.write(plaintext.c_str(), plaintext.size());
    fout.close();

    // Apply encryption
    system(std::string("openssl enc -aes128 -e -nosalt -in " + filename + " -out " + filename + "_out -kfile k_prime_key").c_str());

    // Read encrypted message
    std::ifstream fin(filename + "_out", std::ios::binary);
    int size = getFileSize(fin);
    char bufferEncryptedMessage[size];
    fin.read(bufferEncryptedMessage, size);
    fin.close();

    system(std::string("rm -rf " + filename).c_str());
    system(std::string("rm -rf " + filename + "_out").c_str());

    // Convert encrypted message to string
    std::string * result = new std::string(bufferEncryptedMessage, size);
    return *result;
}

std::string decryptMessage(std::string& ciphertext)
{
    // Generate a random file name
    std::string filename = "decrypt_";
    char buffer;
    for (int i = 0; i < 3; i++)
    {
        buffer = rand() % ('Z' - 'A') + 'A';
        filename += buffer;
    }
    std::ofstream fout(filename, std::ios::binary);

    // Write the data to the file
    fout.write(ciphertext.c_str(), ciphertext.size());
    fout.close();

    // Apply encryption
    system(std::string("openssl enc -aes128 -d -nosalt -in " + filename + " -out " + filename + "_out -kfile k_prime_key").c_str());

    // Read decrypted message
    std::ifstream fin(filename + "_out", std::ios::binary);
    int size = getFileSize(fin);

    char bufferDecryptMessage[size];
    fin.read(bufferDecryptMessage, size);
    fin.close();

    system(std::string("rm -rf " + filename).c_str());
    system(std::string("rm -rf " + filename + "_out").c_str());

    // Convert decrypted message to string
    std::string * result = new std::string(bufferDecryptMessage, size);
    return *result;
}

int main() 
{
    int aToB[2];
    int bToA[2];
    pipe(aToB);
    pipe(bToA);

    int pid = fork();

    if (pid != 0)
    {
        srand(time(0));
        // A
        close(aToB[0]);
        close(bToA[1]);

        // Send signal to B
        std::string operationType("ecb", 3);
        std::cout << "Hello! Please choose between ECB or CFB methods: ";
        std::cin >> operationType;
        while (operationType != "cfb" && operationType != "ecb")
        {
            std::cout << "Invalid operation [" + operationType +"]. Please choose between ECB and CFB methods: ";
            std::cin >> operationType;
        }

        // Tell B what type of operation was selected
        write(aToB[1], operationType.c_str(), 3);

        // Get the encrypted K key
        KeyManager km;
        std::string receivedKey = km.getK();

        // Send it to B: first size, then key
        int size = receivedKey.size();
        write(aToB[1], &size, sizeof(int));
        write(aToB[1], receivedKey.c_str(), receivedKey.size());

        // Write the key to a file
        std::ofstream fout("enc_k_key_A", std::ios::binary);
        fout.write(receivedKey.c_str(), receivedKey.size());
        fout.close();

        // Decrypt it
        system("openssl enc -aes-128-cbc -d -in enc_k_key_A -out k_key_A -kfile k_prime_key");
        char bufferDecryptedK[16];
        
        std::ifstream fin("k_key_A", std::ios::binary);
        fin.read(bufferDecryptedK, 16);

        std::string decryptedKey(bufferDecryptedK, 16);

        system("rm -rf k_key_A");
        system("rm -rf enc_k_key_A");

        // Wait for a message from B
        char messageBuffer;
        read(bToA[0], &messageBuffer, 1);

        // Now we can start the communication

        // Read message from input file
        std::ifstream inputFin(INPUT_FILE);
        std::string message;
        
        // WHITE SPACES ARE NOT READ WATCH OUT IT'S C/C++ NOT PYTHON LUUUUUUUUUUUUUUUUUUUUUUUUUUUL
        inputFin >> message;


        std::cout << "[A] Read message at start: [" + message + "]\n";

        // Check padding (append NULLs if it not ok)
        if (message.size() % 15 != 0)
        {
            int nullsToAppend = 15 - message.size() % 15;
            for (int i = 0; i < nullsToAppend; i++)
                message += APPEND_CHAR;
                // message += ' ';
        }

        if (operationType == "ecb")
        {

            for (int i = 0; i < message.size(); i += 15)
            {
                // Take each block into a new string
                std::string block = message.substr(i, 15);
                std::cout << "Now sending block [" + block + "]...\n";

                std::string encryptedBlock = encryptMessage(block);
                // Encrypted block should have size 16 bytes
                
                write(aToB[1], encryptedBlock.c_str(), 16);
            }
        }
        else
        {
            // Prep the initialization vector
            std::string iv;
            iv.resize(15);
            char ivBuffer;
            for (int i = 0; i < 15; i++)
            {
                getentropy(&ivBuffer, 1);
                iv[i] = ivBuffer;
            }

            std::string startBlock = iv;
            for (int i = 0; i < message.size(); i += 15)
            {
                // Encrypt the startBlock (iv/last ciphertext) with AES-128
                std::string encryptedBlock = encryptMessage(startBlock);

                // Xor it with the key K
                for (int j = 0; j < encryptedBlock.size(); j++)
                    encryptedBlock[j] = encryptedBlock[j] ^ KPrime::getSingleton()->get()[j];

                // Xor it with plaintext


                // Take each block into a new string
                std::string block = message.substr(i, 15);
                std::cout << "Now sending block [" + block + "]...\n";


                
                
                write(aToB[1], encryptedBlock.c_str(), 16);
            }
        }
    }
    else
    {
        srand(time(0));
        // B
        close(aToB[1]);
        close(bToA[0]);

        // Listen to the operation type
        char operatationTypeBuffer[3];
        read(aToB[0], operatationTypeBuffer, 3);

        std::string operationType(operatationTypeBuffer, 3);

        // Listen to the encrypted K
        int size;
        read(aToB[0], &size, sizeof(int));
        char bufferReceivedKey[size];
        read(aToB[0], bufferReceivedKey, size);

        std::string receivedKey(bufferReceivedKey, size);

        // Write the key to a file
        std::ofstream fout("enc_k_key_B", std::ios::binary);
        fout.write(receivedKey.c_str(), receivedKey.size());
        fout.close();

        // Decrypt it
        system("openssl enc -aes-128-cbc -d -in enc_k_key_B -out k_key_B -kfile k_prime_key");
        char bufferDecryptedK[16];
        
        std::ifstream fin("k_key_B", std::ios::binary);
        fin.read(bufferDecryptedK, 16);

        std::string decryptedKey(bufferDecryptedK, 16);

        system("rm -rf k_key_B");
        system("rm -rf enc_k_key_B");

        // Send a message to A
        bool ok = true;
        write(bToA[1], &ok, 1);

        // Now we can start the communication
        
        // Write message to output file
        std::ofstream outputFout(OUTPUT_FILE, std::ios::binary);
        char message[16];

        if (operationType == "ecb")
        {

            while (read(aToB[0], message, 16))
            {
                std::string encryptedMessage(message, 16);

                std::string originalMessage = decryptMessage(encryptedMessage);
                // Output the message to file
                for (int i = 0; i < originalMessage.size(); i++)
                    if (originalMessage[i] != APPEND_CHAR)
                        outputFout << originalMessage[i];
                    else
                        break;
            }
        }
        else
        {
            
            

        }
        
    }
    return 0;
}