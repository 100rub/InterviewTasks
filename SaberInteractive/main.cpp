// (c) Сторублевцев Никита Владимирович 
//     24.01.23

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <random>


//Task 1 ~= 1 hour
void PrintBinary(const int n)
{
    std::string result;
    result.reserve(sizeof(n) * 8);

    int number = n;

    //typical decimal-to-binary algrorithm through euclidean division
    while (number > 1)
    {
        int remainder = (number % 2);
        result = (char)(remainder + 48) + result; // non-STL way of converting int to correct character and appending to front
        number = (number - remainder) / 2;
    }
    result = (char)(number + 48) + result; // appending what is left

    //pad with extra zeroes up to required length
    for (int i = (sizeof(n) * 8) - result.length(); i > 0; i--)
        result = '0' + result;

    std::cout << result << std::endl;
}

//a less comprehensible, but possibly faster solution through binary operations
void PrintBinary2(const int n)
{
    std::string result;
    result.reserve(sizeof(n) * 8);

	// we dont need to reverse the string this way
    for (int i = (sizeof(n) * 8) - 1; i >= 0; i--)
    {
        result += ((n & (1 << i)) ? '1' : '0'); 
    }
    std::cout << result << std::endl;
}



//Task 2 ~= 2 hours;
void RemoveDups(char* str)
{
    // first we need to figure out the length of the string
    int length = 0;
    while (str[length] != '\0')
        length++;

	// now we just go from the back end, shifting tail whenever we encounter duplicate characters
	size_t i = length - 1;
    while (i > 0)
    {
        if (str[i - 1] == str[i])
        {
            for (int j = i; j < length; ++j)
            {
                str[j] = str[j + 1];
            }

            --length;
        }
        
        --i;
    }
}



//Task 3 ~= 16 hours
struct ListNode // структуру ListNode модифицировать нельзя
{
    ListNode* prev = nullptr; // указатель на предыдущий элемент списка, либо `nullptr` в случае начала списка
    ListNode* next = nullptr;
    ListNode* rand = nullptr; // указатель на произвольный элемент данного списка, либо `nullptr`

    std::string data; // произвольные пользовательские данные
};

class List
{
public:
    void Serialize(FILE* file); // сохранение списка в файл, файл открыт с помощью `fopen(path, "wb")`
    void Deserialize(FILE* file); // восстановление списка из файла, файл открыт с помощью `fopen(path, "rb")`

    // ... ваши методы для заполнения списка
    void Print();
    void Push_back(ListNode* node);
    void Generate(int seed);
    

private:
    ListNode* head = nullptr;
    ListNode* tail = nullptr;
    int count = 0;
};

/*
    binary file format
 
    size_t count
    =====node=====
    intptr_t self
        intptr_t prev
        intptr_t next
        intptr_t rand
        size_t data_size
        char* data
    ==============
    intptr_t head
    intptr_t tail
*/

void List::Serialize(FILE* file)
{
    //std::cout << "++++++++++++++SERIALIZING++++++++++++++" << std::endl;

    if (file == nullptr)
    {
        std::cout << "Invalid file pointer" << std::endl;
        return;
    }
    
    // we will serialize nodes in memory address order so we need to sort them as such
    std::vector<ListNode*> addr_vector;
    auto current_node = head;
    while (current_node != nullptr)
    {
        addr_vector.push_back(current_node);
        current_node = current_node->next;
    }
    std::sort(addr_vector.begin(), addr_vector.end());

    // writing the list size to the file
    fwrite(&count, sizeof(int), 1, file);
    //std::cout << count << std::endl;

    // writing each node to the file
    for (auto& node : addr_vector)
    {
        // we are converting pointers to intptr_t because it is guaranteed to be big enough to hold a pointer value on any platform
        // this does NOT on its own solve the problem of serialized data portability between diffirent platforms, 
        // but accounting for this a lot of extra work, which is hopefully unnecessary in this case

        {// writing node self address to file
            intptr_t self = (intptr_t)node;
            fwrite(&self, sizeof(intptr_t), 1, file);
            //std::cout << node << std::endl;
        }

        {// writing prev to the file
            intptr_t prev = (intptr_t)(node->prev);
            fwrite(&prev, sizeof(intptr_t), 1, file);
        }

        {// writing next to the file
            intptr_t next = (intptr_t)(node->next);
            fwrite(&next, sizeof(intptr_t), 1, file);
        }

        {// writing rand to the file
            intptr_t rand = (intptr_t)(node->rand);
            fwrite(&rand, sizeof(intptr_t), 1, file);
        }

        {// writing user data to the file
            size_t data_size = node->data.size();
            fwrite(&data_size, sizeof(size_t), 1, file);
            fwrite(node->data.c_str(), sizeof(char), data_size, file);
            //std::cout << "  data size: " << data_size << std::endl;
            //std::cout << "  data: " << node->data << std::endl;
        }

        //std::cout << "================" << std::endl;
    }

    //std::cout << "================" << std::endl;
    
    {//writing head pointer to the file
        intptr_t head_p = (intptr_t)(this->head);
        fwrite(&head_p, sizeof(intptr_t), 1, file);
        //std::cout << head_p << std::endl;
    }

    {// writing tail pointer to the file
        intptr_t tail_p = (intptr_t)(this->tail);
        fwrite(&tail_p, sizeof(intptr_t), 1, file);
        //std::cout << tail_p << std::endl;
    }
    
    fflush(file);
}

void List::Deserialize(FILE* file)
{
    //std::cout << "+++++++++++++DESERIALIZING+++++++++++++" << std::endl;

    if (file == nullptr)
    {
        std::cout << "Invalid file pointer" << std::endl;
        return;
    }

    //reading list size from the file
    int new_count = -1;
    fread(&new_count, sizeof(int), 1, file);
    this->count = new_count;
    //std::cout << this->count << std::endl;

    // we will assume we care about retaining in-memory order of nodes in addition to just relations
    // (if not, this loop can just be combined with the next one and vector dropped)
    // we pre-create nodes and sort them by adress value, same as during serialization
    std::vector<ListNode*> node_vector;
    for (int i = 0; i < new_count; ++i)
    {
        node_vector.push_back(new ListNode);
    }
    std::sort(node_vector.begin(), node_vector.end());

    // map which we will be using to associate old adresses with the new
    std::map<intptr_t, ListNode*> addr_map;

    // now that we have our shell-nodes in correct memory order, we can read fill out the adress map
    // and fill the nodes themselves with user data from the file
    // we'll have to store some extracted addresses separately for now, since we cant convert them immediately (since the map is still being filled)
    std::vector<intptr_t> prevs;
    std::vector<intptr_t> nexts;
    std::vector<intptr_t> rands;

    for (int i = 0; i < new_count; ++i)
    {
        {// reading the old self address and converting it
            intptr_t self = 0;
            fread(&self, sizeof(intptr_t), 1, file);
            addr_map[self] = node_vector[i]; // this will help us untangle the list structure later
            //std::cout << "self: " << (ListNode*)self << " => " << addr_map[self] << std::endl;
        }

        {// reading the old prev and storing it for later
            intptr_t prev = 0;
            fread(&prev, sizeof(intptr_t), 1, file);
            prevs.push_back(prev);
            //std::cout << "  prev: " << (ListNode*)prev << std::endl;
        }

        {// reading the old next and storing it for later
            intptr_t next = 0;
            fread(&next, sizeof(intptr_t), 1, file);
            nexts.push_back(next);
            //std::cout << "  next: " << (ListNode*)next << std::endl;
        }

        {// reading the old rand and storing it for later
            intptr_t rand = 0;
            fread(&rand, sizeof(intptr_t), 1, file);
            rands.push_back(rand);
            //std::cout << "  rand: " << (ListNode*)rand << std::endl;
        }

        {// reading user data from the file and assigning it to the respective node
            size_t new_data_size = -1;
            fread(&new_data_size, sizeof(size_t), 1, file);
            
            char* new_data_array = new char[new_data_size];
            fread(new_data_array, sizeof(char), new_data_size, file);

            std::string new_data;
            new_data.assign(new_data_array, new_data_array + new_data_size);
            node_vector[i]->data = new_data;

            //std::cout << "  data size: " << node_vector[i]->data.size() << std::endl;
            //std::cout << "  data: " << node_vector[i]->data << std::endl;
        }
        
        //std::cout << "================" << std::endl;
    }

    // now we have a fully-filled address map and can convert all the addresses
    for (int i = 0; i < new_count; ++i)
    {
        {// converting prev
            if ((ListNode*)prevs[i] != nullptr)
                node_vector[i]->prev = addr_map[prevs[i]];
        }

        {// converting next
            if ((ListNode*)nexts[i] != nullptr)
                node_vector[i]->next = addr_map[nexts[i]];
        }

        {// converting rand
            if ((ListNode*)rands[i] != nullptr)
                node_vector[i]->rand = addr_map[rands[i]];
        }
    }

    //std::cout << "================" << std::endl;

    // now we can read and convert head and tail
    {// reading head address from the file
        intptr_t old_head = 0;
        fread(&old_head, sizeof(intptr_t), 1, file);
        this->head = addr_map[old_head];
        //std::cout << "head: " << old_head << " => " << this->head << std::endl;
    }

    {// reading tail address from the file
        intptr_t old_tail = 0;
        fread(&old_tail, sizeof(intptr_t), 1, file);
        this->tail = addr_map[old_tail];
        //std::cout << "tail: " << old_tail << " => " << this->tail << std::endl;
    }
}

void List::Print()
{
    std::cout << "List size: " << count << std::endl;
    std::cout << "  List head: " << head << std::endl;
    std::cout << "  List tail: " << tail << std::endl;
    std::cout << "==================================" << std::endl;

    auto current_node = head;
    while (current_node != nullptr)
    {
        std::cout << "Node ptr: " << current_node << std::endl;
        std::cout << "  Node prev: " << current_node->prev << std::endl;
        std::cout << "  Node next: " << current_node->next << std::endl;
        std::cout << "  Node rand: " << current_node->rand << std::endl;
        std::cout << "  Node data: " << current_node->data << std::endl;
        std::cout << "  Data size: " << current_node->data.size() << std::endl;
        std::cout << "==================================" << std::endl;

        current_node = current_node->next;
    } 
}

void List::Push_back(ListNode* node)
{
    if (head == nullptr)
    {
        head = node;
        tail = node;
    }
    else
    {
        tail->next = node;
        node->prev = tail;
        tail = node;
    }

    count++;
}

void List::Generate(int seed)
{
    static std::default_random_engine rng(seed);
    std::uniform_int_distribution dist_0_10(0, 10);
    std::uniform_int_distribution dist_1_255(1, 255);

    //we'll gonna generate 10-20 nodes
    int list_size = dist_0_10(rng) + 10;

    //temporary copy of node pointers to avoid having to list-iterate over them again 
    std::vector<ListNode*> nodes;
    
    // Node generation loop
    for (int i = 0; i < list_size; ++i)
    {
        ListNode* new_node = new ListNode();

        // generating user data (1-255 characters-worth) for the node
        size_t data_length = dist_1_255(rng);
        std::string data;
        data.reserve(data_length);
        data.append(data_length, (char)(65 + i));
        new_node->data = data;

        // pushing generated node to the list
        Push_back(new_node);
        nodes.push_back(new_node);
    }

    // generating rand pointers for nodes
    std::uniform_int_distribution dist_list_size(0, list_size -1);
    for (int i = 0; i < list_size; ++i)
    {
        if(dist_0_10(rng) >= 5)
            nodes[i]->rand = nodes[dist_list_size(rng)];
    }
}




int main()
{
    //Task 1
    PrintBinary(125);
    PrintBinary2(125);
    std::cout << "==================================" << std::endl;

    
    //Task 2
    char data[] = "AAA BBB   AAA";
    printf("%s\n", data);
    RemoveDups(data);
    printf("%s\n", data); // "A B A" 
    std::cout << "==================================" << std::endl;

    
    //Task 3
    List test_list;
    test_list.Generate(106);
    test_list.Print();

    std::cout << "==================================" << std::endl;

    FILE* file = fopen("file.bin", "wb");
    test_list.Serialize(file);
    fclose(file);

    file = fopen("file.bin", "rb");
    List new_list;
    new_list.Deserialize(file);
    fclose(file);
    new_list.Print();
    
    std::cin.get();
    return 0;
}
