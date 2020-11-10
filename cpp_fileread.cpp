#include <fstream>
#include <iostream>
#include <string>

using namespace std;

int main()
{
    ifstream alphafile("cpp_fileread_testfile.txt");
    if (!alphafile.is_open()) {
        cout << "Could not open file. Exiting.\n";
        return 0;
    }
    int i;
    string str;
    string pid_searchstring = "PID=";
    string::size_type pos;

    while (alphafile >> str) {
        pos = str.find (pid_searchstring, 0);
        if (pos != string::npos) {
            cout << str.substr(pos + pid_searchstring.length(), str.length() - pid_searchstring.length()) << endl;
            break;
        }
    }
    if (alphafile.eof()) {
        cout << "PID not found" << endl;
    }
    alphafile.close();
    return 1;
}
