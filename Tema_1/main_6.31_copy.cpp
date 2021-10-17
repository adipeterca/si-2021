#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/random.h>
#include <sys/wait.h>
#include <iostream>
#include <fstream>
#include <vector>

/**
 * WORKING COMMUNICATIONS
 * 
 * */

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
        k.resize(128);
        char * kBuffer = new char[128];
        getentropy(kBuffer, 128);
        k = kBuffer;
        delete kBuffer;

        // Write it to file "k_key"
        std::ofstream fout("k_key", std::ios::binary);
        fout.write(k.c_str(), 128);
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

int main() 
{
    int aToB[2];
    int bToA[2];
    pipe(aToB);
    pipe(bToA);

    int pid = fork();

    if (pid != 0)
    {
        // A
        close(aToB[0]);
        close(bToA[1]);

        // Send signal to B
        std::string operationType("ecb", 3);
        // std::cout << "Hello! Please choose between ECB or CFB methods: ";
        // std::cin >> operationType;
        // while (operationType != "cfb" && operationType != "ecb")
        // {
        //     std::cout << "Invalid operation [" + operationType +"]. Please choose between ECB and CFB methods: ";
        //     std::cin >> operationType;
        // }

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
        char bufferDecryptedK[128];
        
        std::ifstream fin("k_key_A", std::ios::binary);
        fin.read(bufferDecryptedK, 128);

        std::string decryptedKey(bufferDecryptedK, 128);

        // system("rm -rf k_key_A");
        system("rm -rf enc_k_key_A");

        // Now we can start the communication

    }
    else
    {
        // B
        close(aToB[1]);
        close(bToA[0]);

        // Listen to the operation type
        char buffer[3];
        read(aToB[0], buffer, 3);

        std::string operationType(buffer, 3);

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
        system("openssl enc -aes-128-cbc -d -in enc_k_key_B -out k_key_B -kfile k_prime_key_2");
        char bufferDecryptedK[128];
        
        std::ifstream fin("k_key_B", std::ios::binary);
        fin.read(bufferDecryptedK, 128);

        std::string decryptedKey(bufferDecryptedK, 128);

        // system("rm -rf k_key_B");
        system("rm -rf enc_k_key_B");

        // Now we can start the communication
        
    }
    return 0;
}