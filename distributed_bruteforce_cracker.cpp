/* build using gcc version 9.3.0 and MPICH version 3.3.2 */

#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <cstdlib>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>
#include <cmath>
#include <mpi.h>
#include <crypt.h>
#include <fstream>

/* current namespace */
using namespace std;

/*
    this function extracts the hash from the file. Salt is then seperated from that hash and both are returned as pointer to string variables.
    function also checks if the provided path is correct and that the required user exists within that file. navigation and extraction is done
    using substr().
*/

void getHashAndSalt(string user, string file_name, string *value_h, string *value_s){
    /* variable of type input filestream is declared. */
    ifstream file;
    string st = "";

    file.open(file_name);
    /* condition which checks if the file opened successfully. */
    if(!file){
        cout << "unable to open file" << endl;
        exit(0);
    }
    /* count variable which is used to identify the user, by comparing matching characters. */
    int count = 0;
    bool exists = false;
    /* continuous insertion to the string variable untill the end of file. */
    while(file >> st){
        /* progression after the first character matches. */
        if(st[0] == user[0]){
            for(int i = 1; i < user.length(); i++){
                if(st[i] != user[i])
                    break;
                count++;
            }
            /* condition to verify user, based on matching characters. */           
            if(count == user.length()-1){
                /* bool variable to indicate if the user exists. */
                exists = true;
                break;
            }
            /* value to count is reset if a false positive match was encountered. */
            else{
                count = 0;
            }
        }
    }
    /* if the user was not found in the file then the program exits. */
    if(!exists){
        cout << "No such user exists in the provided file" << endl;
        exit(0);
    }
    /* substring operation to strip username and colon from the string. */
    st = st.substr(st.find(":")+1, st.length()-1);
    /* substring operation to strip the unecessary items attached after the hash. */
    st = st.substr(0, st.find(":"));

    /* complete hash is assigned to the string pointer passed as argument. */
    *value_h = st;

    /* count reset to be reused for salt extraction. */
    count = 0;
    /* loop over the hash string to obtain salt. */
    for(int i = 0; i < st.length(); i++){
        /* condition is check for $ within the hash string. */
        if(st[i] == '$')
            count++;
        /* after $ is encountered 3 times in the string, string up till that part is extracted as salt. */
        if(count == 3){
            /* salt value is assigned to the string pointer passed to the function. */
            *value_s = st.substr(0, i+1);
            /* loop breaks */
            break;
        }
    }
    /* file is closed. */
    file.close();
    
}

/*
    this function generates strings, calculate their hashes with the provided salt and then compares it with hash from the file.
    funtion iterartively goes over each possible combination between the given range provided to it. logic of asynchronous receive
    and testing is implemented within this function. if a matching hash is found then function returns the corresponding string,
    if it receives the termination message from some other process then it just displays the number of combinations tried. 
*/

string permuteNTry(char prime, char end, MPI_Request *req, int *flag, MPI_Status *status, const char *salt, const char *hash){ 
    string str, end_str;
    /* starting character of the character range is assigned to a string variable. */
    str = prime;
    /* ending character of the character range is assigned to another string variable. */
    end_str = end;
    /* count variable to keep track of character iterations. */
    int count = 0;
    /* char pointer variable which points the string which is to be processed.
    this is done because string variables cannot be passed to crypt(). */
    char *str_sub = &str[0];

    /* crypt_data structure variable declared, which is required in crypt_r() thread safe version. */
    struct crypt_data data;
    /* property of data variable is initialiazed. */
    data.initialized = 0;
    /* variable to count combinations tried. */
    long double combinations = 0;

    /* loop which generates the last string within a character string. appending 7 z's to the start character will form the last string. */
    for(int i = 0; i < 8; i++){
        end_str += 'z';
    }
    /* this loop generates strings, calculates and compare hash and tests if some other process has found the hash or not. */
    while(!*flag){
        /* loop which increaments the last item of the string up till z character. */
        while(str[str.length()-1] <= 'z'){
            /* each formed string is hashed with the salt and then compared. strcmp is used to compare hashes which actually are char pointers. */
            if(strcmp(crypt_r(str_sub, salt, &data), hash) == 0){
                /* if a match is found, formed string is returned. */
                cout << "Total combinations tried: " << combinations << endl;
                return str;
            }
            /* combination value incremented. */
            combinations++;
            /* last item of sting is incremented. */
            str[str.length()-1]++;
        }
        /* last item of string is decremented by one. as by the while condition it will cross z character by one on the last iteration. */
        str[str.length()-1]--;
        /* condion to check if the stopping string is reached. after this the size of the string will be greater than 8 characters. */
        if(str == end_str){
            cout << "Total combinations tried: " << combinations << endl;
            /* flag _EOS_ indicating end of search is returned. */
            return "_EOS_";
            // return str;
        }
        /* loop to increment the next non-z value in the string untill it reaches z, each time last item of string reaches z. here string
           behavior is treated as numerical values. 
        */
        for(int i = 0; i < str.length(); i++){
            if(str[i] < 'z'){
                count = 0;
                break;
            }
            else
                count++;
        }
        /* string values are reset after iterations for a range completes. value of count variable is used to realize if the range is complete. */
        if(count == str.length()){
            str[0] = prime;
            if(str.length() > 1)
                for(int i = 1; i < str.length(); i++){
                    str[i] = 'a';
                }
            str += 'a';
            /* count value is reset to count the string items turning z for this newly formed range. */
            count = 0;
        }
        /* this else will just increment the next non-z character by one within the character range. */
        else
            if(str.length() >= 2)
                for(int i = str.length()-2; i >= 0; i--){
                    if(str[i] < 'z'){
                        str[i]++;
                        break;
                    }
                    /* if the end value of the range is reached then the z is turned to a. */
                    else if(str[i] == 'z'){
                        str[i] = 'a';
                    }

                }
        /* last item of the string is reset to a. the same cycle continues. */
        str[str.length()-1] = 'a';

        /* test function to check if the match is already found. */
        MPI_Test(req, flag, status);
    }
    /* prints the total combinations tried. */
    cout << "Total combinations tried: " << combinations << endl;
    /* terminate flag is returned if match is found by another process, current process breaks out of while loop. */
    return "_TERMINATE_";
}

/* function calculates the fixed character range for each process based on the number of processes. */
int division(float num_of_processes){
    /* upper absolute value of the result of division is returned. */
    int val = ceil(26/num_of_processes);
    return val;
}


/* main function. */
int main(int argc, char *argv[]){

    /* necessary variables are declared and some of them are initialized. */
    int world_size, world_rank, name_length;
    /* character range numeric value. */
    int chr_range;
    /* char array that will contain the first and last character of the range. */
    char ch_msg[2];
    /* buffers for indication of match found by some process. */
    int termination_recv_buffer[2];
    int termination_snd_buffer[2] = {1,1};
    /* flag for testing against an asynchronous call. */
    int termination_flag = 0;
    /* char array to obtain the processor name. */
    char processor_name[MPI_MAX_PROCESSOR_NAME];
    string chr_password;
    /* string and const char variables to handle the values of hash and salt. */
    string str_h, str_s;
    const char *hash;
    const char *salt;
    /* string variables for file name and the name of the user within the file. */
    string user, file_name;
    /* MPI variables required in some of the calls. */
    MPI_Request request; 
    MPI_Status status; 
    /* MPI initialization. */
    MPI_Init(NULL, NULL);
    /* populating the variable with the number of processes. */
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    /* populating the variable with the rank, seperate for each process. */
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Get_processor_name(processor_name, &name_length);
    
    /* buffers for MPI_Gather call to determine the state of result. */
    int s_buff[1] = {0};
    int r_buff[world_size];

    /* to signal safe premature exit signal to other nodes. all nodes must exit properly in the cluster. */
    int exit_signal;
    
    /* variable of type clock. used to timing the total execution. */
    clock_t start_time;
    /* part exclusive to process 0. */
    if(world_rank == 0){
        /* 
        signal initialiazed as correct state of execution uptill now. the moment it flips means 
        that all processes must terminated without further execution
        */
        exit_signal = 0;
        /* check for correct commandline arguments. */
        if(argc < 2){
            cout << "Please provide required arguments, '--help' for help" << endl;
            exit_signal = 1;
        }
        if(!exit_signal){
            if((string)argv[1] != "--help" && (string)argv[1] != "--user"){
                cout << "Please provide required arguments, '--help' for help" << endl;
                exit_signal = 1;
            }
        }
        if(!exit_signal){
            /* displaying of help message if --help flag is passed in the commandline. */
            if((string)argv[1] == "--help"){
                cout << "This is a password breaking utility. It can break passwords containing only lowercase characters, upto 8 characters long. Hashes must be encrypted using SHA 512" << endl;
                cout << "'--user' argument is for providing linux user, for which password is to be cracked. NOTE: must run as root" << endl;
                cout << "'--hash-file' argument is to provide the file containing the hash which is to be cracked (default for /etc/shadow)" << endl;
                cout << "Both of the above mentioned arguments are required" << endl;
                exit_signal = 1;
            }
        }
        if(!exit_signal){
            /* validations of --user flag. */
            if((string)argv[1] == "--user"){

                if(argc < 3){
                    cout << "Please provide username with flag --user" << endl;
                    exit_signal = 1;
                }

            }
        }
        if(!exit_signal){
            /* message if some commandline arguments are missing. */
            if(argc < 4){
                cout << "Please provide --hash-file argument (default for /etc/shadow)" << endl;
                exit_signal = 1;
            }
        }
        if(!exit_signal){
            if(argc < 5){
                cout << "Please provide file name with flag --hash-file" << endl;
                exit_signal = 1;
            }
        }

        if(!exit_signal){
            if((string)argv[3] != "--hash-file"){
                cout << "Please provide --hash-file flag" << endl;
                exit_signal = 1;
            }
        }

        if(!exit_signal){
            /* if the /etc/shadow file is to be red then running in root is must. condition varifies that. */
            if((string)argv[4] == "default"){
                if(getuid()){
                    cout << "For default file, you must run as root" << endl;
                    exit_signal = 1;
                }
                else
                    cout << endl << "File: /etc/shadow" << endl;
            }
        }

        if(!exit_signal){
            /* variable is assigned the range of characters. */
            chr_range = division((float) world_size);
            
            /* char variable is used to form characters and pass in form of buffer. */
            char ch = 'a';
            /* loop whcih forms starting and ending characters and pass to each process using buffer. */
            for(int j = world_size-1; j >= 0; j--){
                ch_msg[0] = ch;
                for(int i = 0; i < chr_range; i++){
                    if(ch <=  'z')
                        ch++;
                }
                ch_msg[1] = --ch;
                ch++;
                if(ch > 'z')
                    ch = 'a';
                /* process 0 sends to everyone else. itself excluded, it will get the last range formed. */
                if(j != 0)
                    MPI_Send(ch_msg, 2, MPI_CHAR, j, 1, MPI_COMM_WORLD);
            }
            /* clock type variable initialized. */
            start_time = clock();
        }
    }
    
    /* 0 process must dictate the further course of execution. */
    MPI_Bcast(&exit_signal, 1, MPI_INT, 0, MPI_COMM_WORLD);
    /* safe exit if some commandline argument is missing or incorrect. */
    if(exit_signal){
        return 0;
    }

    /* variable is assigned the string equilavant of the appropiate commandline argument. */
    user = (string)argv[2];
    
    /* proper file path is assigned to the variable as stated by the user in the commandline args. */
    if((string)argv[4] == "default")
        file_name = "/etc/shadow";
    else
        file_name = (string)argv[4];
    
    /* string variables are populated with hash and salt. */
    getHashAndSalt(user+":", file_name, &str_h, &str_s);
    /* const char pointers are assigned the values of hash and salt. necessary for the crypt function. */
    hash = str_h.c_str();
    salt = str_s.c_str();

    /* displaying the salt and hash, a degree of confirmation that values extracted are correct. */
    if(world_rank == 0){
        cout << "Salt: " << salt << endl;
        cout << "Hash:" << hash << endl << endl;
        cout << endl << "Search Initiated..." << endl << endl;
    }

    /* all processes except 0 receive their share of character ranges. */
    if(world_rank != 0){
        MPI_Recv(ch_msg, 2, MPI_CHAR, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    }

    /* asynchronous receive call. */
    MPI_Irecv(termination_recv_buffer, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &request);
    /* variable receives the string value or some flag based on the state of execution of the current process. */
    chr_password = permuteNTry(ch_msg[0],ch_msg[1], &request, &termination_flag, &status, salt, hash);

    /* condition block if password is found. */
    if(chr_password != "_TERMINATE_" && chr_password != "_EOS_"){
        /* notify all other processes to stop using a MPI_Send call. */
        for(int i = 0; i < world_size; i++){
            
            if(i != world_rank)
                MPI_Send(termination_snd_buffer, 1, MPI_INT, i, 2, MPI_COMM_WORLD);
        }

        /* buffer turned to 1 indicating some process has successfully found the password. */
        s_buff[0] = 1;

        /* if the founding process is 0 it will change the value in receiving buffer beforehand. makes it easy to use gather call on it.*/
        if(world_rank == 0)
            r_buff[0] = 1;

        /* message displaying information about process and the password. */
        cout << "Process (" << getpid() << ") with rank: " << world_rank << " found password -> " << chr_password << endl;
    }

    /* condition block for if some other flag is returned. */
    else{
        /* if the search ended. */
        if(chr_password == "_EOS_")
            cout << "Process (" << getpid() << ") with rank: " << world_rank << " search ended" << endl;
        /* if the process terminated because of a call from some other process. */
        else
            cout << "Process (" << getpid() << ") with rank: " << world_rank << " terminated" << endl;
    }
    /* information about the range received by the current process. */
    cout  << "Process (" << getpid() << ") with rank: " << world_rank << " received range " << ch_msg[0] << " - " << ch_msg[1] << endl;
    cout << "--------------------------------------------------------------" << endl;

    /* gather call to collect responses from each process. in case all the processes ended and password was not found. */
    MPI_Gather(s_buff, 1, MPI_INT, r_buff, 1, MPI_INT, 0, MPI_COMM_WORLD);

    /* process 0 checks if password was found by some process and displays appropiate message if not. */
    if(world_rank == 0){
        
        bool found = false;
        /* this crazy initialization because simple was just not working for some reason. */
        int *a = (int*)malloc(sizeof(int));
        
        /* loop to check the values of the receive buffer collected from MPI_Gather call. if 1 is present then it means some process found the pass. */
        for(*a = 0; *a < world_size; *a = *a+1){
            if(r_buff[*a] == 1){
                found = true;
                break;
            }
        }
        /* message if none of the processes were successful. */
        if(!found){
            cout << "Password could not be found, it may be because of password containing characters other than lower case alphabets or lenght may be more than 8 characters " << endl;
        }
        /* prints the total execution time in seconds. */
        cout << endl << "Approximate execution time: " << (double)(clock() - start_time)/CLOCKS_PER_SEC << " seconds" << endl << endl;
    }
    /* terminates MPI execution environment, all processes must call this routine before exiting. */
    MPI_Finalize();

    return 0;
}