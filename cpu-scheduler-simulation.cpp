

#include <iostream>
#include <vector>
#include <deque> 
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

using namespace std;

/* Constants */
const int MAX_TIME = 500;       // The maximum time for the simulation
const int IN_USE = 5;           // The number of processes that can be in use at once
const int HOW_OFTEN = 25;       // How often to print the system state
const int DEFAULT_QUANTUM = 5;   // The default time quantum for Round Robin

/* Algorithms */
enum Algorithm { RR, SJF, PRIO, COMPARE };
Algorithm current_algo = RR;
bool compare_mode = false;
string input_filename;

/* Process Structure */
struct Process {
    string process_name;                // The name of the process
    int process_id;                     // The ID of the process
    int arrival_time;                   // The time the process arrives

    vector<pair<char, int>> history;    // Vector to store the process's history

    int history_index = 0;              // Index into the history vector

    int cpu_timer = 0;                  // Timer for CPU usage
    int input_timer = 0;                // Timer for input usage
    int output_timer = 0;               // Timer for output usage

    int total_cpu_time = 0;             // Total time spent on CPU
    int total_input_time = 0;           // Total time spent on input
    int total_output_time = 0;          // Total time spent on output

    int cpu_count = 0;                  // Number of times the process has used the CPU
    int input_count = 0;                // Number of times the process has used input
    int output_count = 0;               // Number of times the process has used output

    int waiting_time = 0;               // Total time spent waiting in ready queue

    int first_run_time = -1;            // Time when the process first ran on the CPU
    int completion_time = -1;           // Time when the process finished

    int priority = 10;                  // Priority (lower number = higher priority)
};

/* Queues */
deque<Process*> entry_queue;        // Queue for new processes
deque<Process*> ready_queue;        // Queue for ready processes
deque<Process*> input_queue;        // Queue for processes waiting for input
deque<Process*> output_queue;       // Queue for processes waiting for output

/* Active Processes */
Process* active = nullptr;          // Pointer to the currently running process
Process* input_active = nullptr;    // Pointer to the currently waiting for input process
Process* output_active = nullptr;   // Pointer to the currently waiting for output process

vector<Process*> all_processes;                     // Vector to store all created processes
vector<pair<string, pair<int, int>>> gantt_chart;   // Vector to store Gantt chart intervals

/* Simulation Statistics */
int sys_timer = 0;                  // The system timer
int quantum = DEFAULT_QUANTUM;      // The time quantum for Round Robin
int quantum_counter = 0;            // The current time within the quantum
int idle_ticks = 0;                 // The number of idle ticks
int total_done = 0;                 // The total number of processes that have completed

int total_waiting_time = 0;         // The total time all processes have spent waiting
int total_cpu_time = 0;             // The total time all processes have spent on the CPU
int total_input_time = 0;           // The total time all processes have spent on input
int total_output_time = 0;          // The total time all processes have spent on output

/* Function Prototypes */
void read_input_file(string filename);
void run_simulation();
void check_arrivals();
void dispatch_cpu();
void dispatch_io();
void execute_cpu();
void execute_input();
void execute_output();
void update_waiting_time();
void print_state(int t);
void terminate_process(Process* p);

/*********************************************************************************
* read_input_file
*   Function to read the input file and populate the entry queue with processes
*
* @param filename - The name of the input file to read
*********************************************************************************/
void read_input_file(string filename) {
    ifstream infile(filename);
    if (!infile.is_open()) {
        cout << "Failed to open " << filename << " file!" << endl;
        exit(1);
    }

    string line;
    int next_pid = 101;

    while (getline(infile, line)) {
        if (line.empty()) continue;
        stringstream ss(line);
        string name;
        if (!(ss >> name)) continue;
        if (name == "STOPHERE") break;

        int arrival;
        if (!(ss >> arrival)) {
            cout << "Warning: Malformed process " << name << " (invalid arrival), skipping." << endl;
            continue;
        }

        Process* p = new Process();
        p->process_name = name;
        p->arrival_time = arrival;
        p->process_id = next_pid++;
        p->history_index = 0;

        bool valid = true;
        if (arrival < 0) {
            valid = false;
        }

        string next_token;
        if (!(ss >> next_token)) {
            valid = false;
        }

        if (valid) {
            if (next_token == "C" || next_token == "I" || next_token == "O") {
                p->priority = 10;
            } else {
                try {
                    size_t pos;
                    p->priority = stoi(next_token, &pos);
                    if (pos != next_token.length()) valid = false;
                } catch (...) {
                    valid = false;
                    p->priority = 10;
                }
                if (!(ss >> next_token)) { valid = false; }
            }
        }

        string token = next_token;
        int val;
        char last_type = ' ';
        
        while (valid && token != "N") {
            if (!(ss >> val)) { valid = false; break; }
            if (val <= 0) { valid = false; }
            if (token != "C" && token != "I" && token != "O") { valid = false; }
            
            char current_type = token[0];
            
            // Processes must start with a CPU burst
            if (last_type == ' ' && current_type != 'C') {
                valid = false;
            }
            
            // Reject consecutive CPU or consecutive I/O bursts
            if (last_type != ' ') {
                if (last_type == 'C' && current_type == 'C') {
                    valid = false;
                } else if ((last_type == 'I' || last_type == 'O') && (current_type == 'I' || current_type == 'O')) {
                    valid = false;
                }
            }

            p->history.push_back(make_pair(current_type, val));
            last_type = current_type;

            if (!(ss >> token)) { valid = false; break; }
        }
        
        if (valid && token == "N") {
            if (ss >> val) {
                if (val != 0) valid = false;
                
                // Reject unexpected trailing tokens after N 0
                string leftover;
                if (ss >> leftover) {
                    valid = false;
                }
            } else {
                valid = false;
            }
        } else {
            valid = false;
        }

        if (valid && !p->history.empty()) {
            entry_queue.push_back(p);
            all_processes.push_back(p);
        } else {
            cout << "Warning: Malformed process " << name << ", skipping." << endl;
            delete p; 
        }
    }
    infile.close();

    stable_sort(entry_queue.begin(), entry_queue.end(), [](Process* a, Process* b) {
        return a->arrival_time < b->arrival_time;
    });
}

/*********************************************************************************
* main 
*   The main function. Loop represents the scheduler in the OS. Timer initialized
*   to '0' and increments until the timer reaches MAX_TIME or there are no more
*   processes left.
*
* @param argc - The number of command line arguments
* @param argv - The array of command line arguments
*********************************************************************************/
void print_simulation_stats() {
    cout << "\n========== Final Simulation Summary ==========" << endl;
    
    // Warn if MAX_TIME was reached and there are still unfinished processes
    bool has_unfinished = !entry_queue.empty() || !ready_queue.empty() || !input_queue.empty() || 
                          !output_queue.empty() || active || input_active || output_active;
                          
    if (sys_timer >= MAX_TIME && has_unfinished) {
        cout << "WARNING: MAX_TIME (" << MAX_TIME << ") reached. Simulation terminated with unfinished processes." << endl;
    }
    
    cout << "Algorithm: ";
    if (current_algo == RR) cout << "Round Robin (Q=" << quantum << ")";
    else if (current_algo == SJF) cout << "Shortest Job First (Non-Preemptive)";
    else if (current_algo == PRIO) cout << "Priority (Non-Preemptive)";
    cout << endl;

    cout << "Final Timer: " << sys_timer << endl;
    cout << "Processes Terminated: " << total_done << endl;
    cout << "CPU Idle Time: " << idle_ticks << endl;

    if (sys_timer > 0) {
        double cpu_utilization = ((double)(sys_timer - idle_ticks) / sys_timer) * 100.0;
        cout << fixed << setprecision(2);
        cout << "CPU Utilization: " << cpu_utilization << "%" << endl;
    }

    double total_turnaround_time = 0;
    double total_response_time = 0;

    cout << "\n--- Per-Process Metrics ---" << endl;
    cout << left << setw(10) << "Process" << setw(6) << "AT" << setw(6) << "CT" 
         << setw(6) << "TAT" << setw(6) << "RT" << setw(6) << "WT" << endl;
    
    for (Process* p : all_processes) {
        if (p->completion_time != -1) {
            int tat = p->completion_time - p->arrival_time;
            int rt = (p->first_run_time != -1) ? (p->first_run_time - p->arrival_time) : -1;
            total_turnaround_time += tat;
            if (rt != -1) total_response_time += rt;

            cout << left << setw(10) << p->process_name 
                 << setw(6) << p->arrival_time 
                 << setw(6) << p->completion_time 
                 << setw(6) << tat 
                 << setw(6) << rt 
                 << setw(6) << p->waiting_time << endl;
        }
    }

    if (total_done > 0) {
        double average_waiting_time = (double)total_waiting_time / total_done;
        double average_tat = total_turnaround_time / total_done;
        double average_rt = total_response_time / total_done;
        cout << "\nAverage Waiting Time: " << average_waiting_time << endl;
        cout << "Average Turnaround Time: " << average_tat << endl;
        cout << "Average Response Time: " << average_rt << endl;
    }

    cout << "\n--- Gantt Chart ---" << endl;
    if (!gantt_chart.empty()) {
        for (size_t i = 0; i < gantt_chart.size(); i++) {
            cout << gantt_chart[i].second.first << "--" << gantt_chart[i].first << "--";
            if (i == gantt_chart.size() - 1) {
                cout << gantt_chart[i].second.second;
            }
        }
        cout << endl;
    }
    cout << "\nTotal CPU Time: " << total_cpu_time << endl;
    cout << "Total Input Time: " << total_input_time << endl;
    cout << "Total Output Time: " << total_output_time << endl;
    cout << "Processes Left in Entry Queue: " << entry_queue.size() << endl;
    cout << "Processes Left in Ready Queue: " << ready_queue.size() << endl;
    cout << "Processes Left in Input Queue: " << input_queue.size() << endl;
    cout << "Processes Left in Output Queue: " << output_queue.size() << endl;
    cout << "===============================================" << endl;
}

void reset_simulation(const string& filename) {
    sys_timer = 0;
    idle_ticks = 0;
    total_done = 0;
    total_waiting_time = 0;
    total_cpu_time = 0;
    total_input_time = 0;
    total_output_time = 0;
    quantum_counter = 0;
    active = nullptr;
    input_active = nullptr;
    output_active = nullptr;
    entry_queue.clear();
    ready_queue.clear();
    input_queue.clear();
    output_queue.clear();
    for (Process* p : all_processes) {
        delete p;
    }
    all_processes.clear();
    gantt_chart.clear();
    read_input_file(filename);
}

void run_and_print_summary(Algorithm algo, int q) {
    current_algo = algo;
    quantum = q;
    run_simulation();
    print_simulation_stats();
}

int main (int argc, char* argv[]){
    if (argc < 2) {
        cout << "Usage: ./simulator <file> [algorithm] [quantum]" << endl;
        cout << "Algorithms: RR, SJF, PRIO, COMPARE" << endl;
        return 1;
    }
    input_filename = argv[1];

    if (argc >= 3) {
        string alg_str = argv[2];
        if (alg_str == "SJF") current_algo = SJF;
        else if (alg_str == "PRIO") current_algo = PRIO;
        else if (alg_str == "COMPARE") compare_mode = true;
        else if (alg_str == "RR") current_algo = RR;
        else {
            cout << "Invalid algorithm. Using RR." << endl;
            current_algo = RR;
        }
    }

    if (argc >= 4) {
        try {
            size_t pos;
            string q_str = argv[3];
            quantum = stoi(q_str, &pos);
            if (pos != q_str.length() || quantum <= 0) {
                cout << "Error: Quantum must be a positive integer." << endl;
                return 1;
            }
        } catch (...) {
            cout << "Error: Invalid quantum value." << endl;
            return 1;
        }
    }

    if (compare_mode) {
        cout << "\n=== COMPARISON MODE ===" << endl;
        reset_simulation(input_filename);
        run_and_print_summary(RR, quantum);

        reset_simulation(input_filename);
        run_and_print_summary(SJF, quantum);

        reset_simulation(input_filename);
        run_and_print_summary(PRIO, quantum);
    } else {
        read_input_file(input_filename);
        run_and_print_summary(current_algo, quantum);
    }

    // Clean up memory
    for (Process* p : all_processes) {
        delete p;
    }
    all_processes.clear();

    return 0;
}

/*********************************************************************************
* run_simulation
*   This is the main loop that runs the whole thing until max time or we
*   run out of processes to run.
*
* @param none
*********************************************************************************/
void run_simulation() {
    // Loop until max time or we run out of process to run
    while (sys_timer < MAX_TIME) {
        check_arrivals();
        
        // Stop if nothing left to do
        if (entry_queue.empty() && ready_queue.empty() && input_queue.empty() && 
            output_queue.empty() && !active && !input_active && !output_active) {
            break;
        }

        dispatch_cpu();
        dispatch_io();

        // Record Gantt chart interval
        string current_run = active ? (active->process_name + "(PID:" + to_string(active->process_id) + ")") : "IDLE";
        if (gantt_chart.empty() || gantt_chart.back().first != current_run) {
            gantt_chart.push_back({current_run, {sys_timer, sys_timer + 1}});
        } else {
            gantt_chart.back().second.second = sys_timer + 1;
        }

        update_waiting_time();

        execute_cpu();
        execute_input();
        execute_output();

        // Print the state every HOW_OFTEN time units
        if (sys_timer % HOW_OFTEN == 0) {
            print_state(sys_timer);
        }

        // Increment the timer
        sys_timer++;
    }
}

/*********************************************************************************
* check_arrivals
*   Checks if any new process arrived in the entry queue and moves them to the ready queue
*   if there is room in the system.
*
* @param none
*********************************************************************************/
void check_arrivals() {
    // Check the entry queue for new arrivals and move them to ready if there is room in the system
    int cur_in_sys = (int)ready_queue.size() + (int)input_queue.size() + (int)output_queue.size();

    // Count the active processes in the system
    if (active) {
        cur_in_sys++;
    }
    if (input_active) {
        cur_in_sys++;
    }
    if (output_active) {
        cur_in_sys++;
    }

    // Move processes from entry to ready if they have arrived and there is room in the system
    while (!entry_queue.empty() && entry_queue.front()->arrival_time <= sys_timer && cur_in_sys < IN_USE) {
        Process* p = entry_queue.front();
        entry_queue.pop_front();
        ready_queue.push_back(p);
        cur_in_sys++;
        cout << "Time " << sys_timer << ": " << p->process_name << " moved to Ready" << endl;
    }
}

/*********************************************************************************
* dispatch_cpu
*   Picks the next thing from the ready queue and puts it on the cpu 
*   to start running.
*
* @param none
*********************************************************************************/ 
void dispatch_cpu() {
    if (active == nullptr && !ready_queue.empty()) {
        auto best_it = ready_queue.begin();

        if (current_algo == SJF) {
            int shortest = (*best_it)->history[(*best_it)->history_index].second;
            for (auto it = ready_queue.begin() + 1; it != ready_queue.end(); ++it) {
                int burst = (*it)->history[(*it)->history_index].second;
                if (burst < shortest) {
                    shortest = burst;
                    best_it = it;
                }
            }
        } else if (current_algo == PRIO) {
            int highest_prio = (*best_it)->priority;
            for (auto it = ready_queue.begin() + 1; it != ready_queue.end(); ++it) {
                if ((*it)->priority < highest_prio) {
                    highest_prio = (*it)->priority;
                    best_it = it;
                }
            }
        }

        active = *best_it;
        ready_queue.erase(best_it);
        
        if (active->first_run_time == -1) {
            active->first_run_time = sys_timer;
        }
        active->cpu_timer = active->history[active->history_index].second;
        quantum_counter = 0;
    }
}

void dispatch_io() {
    if (input_active == nullptr && !input_queue.empty()) {
        input_active = input_queue.front();
        input_queue.pop_front();
        input_active->input_timer = input_active->history[input_active->history_index].second;
    }
    if (output_active == nullptr && !output_queue.empty()) {
        output_active = output_queue.front();
        output_queue.pop_front();
        output_active->output_timer = output_active->history[output_active->history_index].second;
    }
}

void execute_cpu() {
    if (active != nullptr) {
        active->cpu_timer--;
        active->total_cpu_time++;
        quantum_counter++;

        if (active->cpu_timer == 0) {
            active->cpu_count++;
            active->history_index++;

            if (active->history_index >= (int)active->history.size()) {
                active->completion_time = sys_timer + 1;
                terminate_process(active);
                total_done++;
            } else {
                char burst_type = active->history[active->history_index].first;
                if (burst_type == 'I') {
                    input_queue.push_back(active);
                } else if (burst_type == 'O') {
                    output_queue.push_back(active);
                }
            }
            active = nullptr;
        } else if (current_algo == RR && quantum_counter == quantum) {
            active->history[active->history_index].second = active->cpu_timer;
            ready_queue.push_back(active);
            active = nullptr;
        }
    } else {
        idle_ticks++;
    }
}

void execute_input() {
    if (input_active) { 
        input_active->input_timer--;
        input_active->total_input_time++;
        if (input_active->input_timer == 0) {
            input_active->input_count++;
            input_active->history_index++;
            if (input_active->history_index >= (int)input_active->history.size()) {
                input_active->completion_time = sys_timer + 1;
                terminate_process(input_active);
                total_done++;
            } else {
                ready_queue.push_back(input_active);
            }
            input_active = nullptr;
        }
    }
}

void execute_output() {
    if (output_active) {
        output_active->output_timer--;
        output_active->total_output_time++;
        if (output_active->output_timer == 0) {
            output_active->output_count++;
            output_active->history_index++;
            if (output_active->history_index >= (int)output_active->history.size()) {
                output_active->completion_time = sys_timer + 1;
                terminate_process(output_active);
                total_done++;
            } else {
                ready_queue.push_back(output_active);
            }
            output_active = nullptr;
        }
    }
}

/*********************************************************************************
* update_waiting_time
*   Adds to the wait timer for every proccess stuck in a queue.
*
* @param none
*********************************************************************************/
void update_waiting_time() {
    // Add to the waiting time for each process in the ready queue only
    for (int i = 0; i < (int)ready_queue.size(); i++){
        ready_queue[i]->waiting_time++;
    }
}

/*********************************************************************************
* print_state
* Prints out the current state of the system.
*
* @param t - the current time unit
*********************************************************************************/
void print_state(int t) {
    // Print the current state of the system
    cout << "\n--- Time " << t << " ---" << endl;
    cout << "Active: " << (active ? to_string(active->process_id) : "None") << endl;
    cout << "IActive: " << (input_active ? to_string(input_active->process_id) : "None") << endl;
    cout << "OActive: " << (output_active ? to_string(output_active->process_id) : "None") << endl;

    // Print the queues
    cout << "Entry Queue: ";
    if (entry_queue.empty()) cout << "Empty";
    for (size_t i = 0; i < entry_queue.size(); i++) {
        cout << entry_queue[i]->process_id << " ";
    } 
    cout << endl;
    
    // Ready Queue
    cout << "Ready Queue: ";
    if (ready_queue.empty()) {
        cout << "[Empty]";
    } 
    for (size_t i = 0; i < ready_queue.size(); i++) {
        cout << ready_queue[i]->process_id << " ";
    }
    cout << endl;

    // Input Queue
    cout << "Input Queue: ";
    if (input_queue.empty()) {
        cout << "[Empty]";
    }
    for (size_t i = 0; i < input_queue.size(); i++) {
        cout << input_queue[i]->process_id << " ";
    }
    cout << endl;

    // Output Queue
    cout << "Output Queue: ";
    if (output_queue.empty()) cout << "[Empty]";
    for (size_t i = 0; i < output_queue.size(); i++) {
        cout << output_queue[i]->process_id << " ";
    }
    cout << endl;
}
/*********************************************************************************
 * terminate_process
 *   Prints out the stats for a process that is terminating and cleans up memory.
 * 
 * @param p - the process that is terminating
 *********************************************************************************/
void terminate_process(Process* p) {
    // Print the stats for the process that is terminating
    cout << "Time " << p->completion_time << ": " << p->process_name << " terminated" << endl;
    cout << "  - Process ID: " << p->process_id << endl;
    cout << "  - CPU Bursts: " << p->cpu_count << endl;
    cout << "  - Input Bursts: " << p->input_count << endl;
    cout << "  - Output Bursts: " << p->output_count << endl;
    cout << "  - Time in CPU: " << p->total_cpu_time << endl;
    cout << "  - Time in Input: " << p->total_input_time << endl;
    cout << "  - Time in Output: " << p->total_output_time << endl;
    cout << "  - Time Waiting: " << p->waiting_time << endl;

    cout << endl;

    // Add the process's times to the total times
    total_waiting_time += p->waiting_time;
    total_cpu_time += p->total_cpu_time;
    total_input_time += p->total_input_time;
    total_output_time += p->total_output_time;
}