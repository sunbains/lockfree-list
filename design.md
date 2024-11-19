# Lock-Free Doubly Linked List Design

This document details the architecture, states, and linearization points of our lock-free doubly linked list implementation.

## Architecture

### Core Components

```mermaid
classDiagram
    class Node {
        +atomic<Node*> m_next
        +atomic<Node*> m_prev
    }

    class TimestampNode {
        +int value
        +time_point timestamp
        +atomic<uint64_t> access_count
        +age_ms()
        +age_seconds()
        +update_timestamp()
        +record_access()
    }

    class Lock_free_list~T~ {
        -atomic<Node*> m_head
        -atomic<Node*> m_tail
        +push_front(Node*)
        +push_back(Node*)
        +insert_after(Node*, Node*)
        +remove(Node*)
        +find(const T&)
        +iterator begin()
        +iterator end()
    }

    class iterator {
        -Node* m_node
        -Node* m_prev
        +operator++()
        +operator--()
        +operator*()
    }

    Node <|-- TimestampNode
    Lock_free_list o-- Node
    Lock_free_list *-- iterator
```

## State Diagrams

### Insert Operation States

```mermaid
stateDiagram-v2
    [*] --> InitInsert: New Node
    InitInsert --> ValidateTarget: Read Insert Point
    ValidateTarget --> PreparePtrs: Valid
    ValidateTarget --> [*]: Invalid Target

    PreparePtrs --> AttemptCAS: Setup Next/Prev
    AttemptCAS --> UpdateAdjacent: CAS Success
    AttemptCAS --> ValidateTarget: CAS Failed
    
    UpdateAdjacent --> UpdateTail: Is New Tail
    UpdateAdjacent --> Finish: Not Tail
    UpdateTail --> Finish: Update Complete
    Finish --> [*]
```

### Remove Operation States

```mermaid
stateDiagram-v2
    [*] --> InitRemove: Target Node
    InitRemove --> LoadPtrs: Read Links
    LoadPtrs --> ValidateNode: Check Links
    
    ValidateNode --> UpdateNext: Node Valid
    ValidateNode --> [*]: Node Invalid

    UpdateNext --> NextCAS: Not Head
    UpdateNext --> HeadUpdate: Is Head
    
    NextCAS --> UpdatePrev: CAS Success
    NextCAS --> LoadPtrs: CAS Failed
    
    HeadUpdate --> UpdatePrev: Head Updated
    
    UpdatePrev --> PrevCAS: Not Tail
    UpdatePrev --> TailUpdate: Is Tail
    
    PrevCAS --> Finish: CAS Success
    PrevCAS --> LoadPtrs: CAS Failed
    
    TailUpdate --> Finish: Tail Updated
    Finish --> [*]
```

### Search Operation States

```mermaid
stateDiagram-v2
    [*] --> InitSearch: Start Search
    InitSearch --> LoadHead: Read Head
    LoadHead --> ValidateNode: Node Loaded
    LoadHead --> NotFound: List Empty
    
    ValidateNode --> CompareValue: Node Valid
    ValidateNode --> LoadNext: Not Found
    
    CompareValue --> Found: Match
    CompareValue --> LoadNext: No Match
    
    LoadNext --> ValidateNode: Next Exists
    LoadNext --> NotFound: End of List
    
    Found --> [*]: Return Node
    NotFound --> [*]: Return Null
```

## Linearization Points

### Push Front Operation

```mermaid
sequenceDiagram
    participant T as Thread
    participant L as Lock-free List
    participant H as Head
    participant O as Old Head

    rect rgb(240, 240, 255)
        Note over T,O: Preparation Phase
        T->>T: Initialize new node
        T->>H: Read current head
        T->>T: Set new_node->next to head
    end

    rect rgb(255, 220, 220)
        Note over T,H: Linearization Point
        T->>H: CAS(head, old_head, new_node)
        Note right of H: List becomes visible<br/>with new node as head
    end

    rect rgb(220, 255, 220)
        Note over T,O: Post-Linearization
        alt old_head not null
            T->>O: Set old_head->prev
        else empty list
            T->>L: Update tail
        end
    end
```

### Insert After Operation

```mermaid
sequenceDiagram
    participant T1 as Thread 1
    participant List as Lock-free List
    participant T2 as Thread 2

    rect rgb(200, 240, 255)
        Note over T1,List: Insert Operation Linearization
        T1->>List: Read current state
        T1->>T1: Prepare new node
        
        Note right of List: Linearization Point: <br/> Successful CAS of next pointer
        T1->>List: CAS(target.next, old_next, new_node)
        
        T1->>List: Update prev pointers
        Note right of List: Auxiliary updates <br/> (non-linearization point)
    end
```

### Remove Operation

```mermaid
sequenceDiagram
    participant T2 as Thread
    participant List as Lock-free List

    rect rgb(255, 220, 220)
        Note over T2,List: Remove Operation Linearization
        T2->>List: Read node links
        
        Note right of List: Linearization Point: <br/> First successful CAS <br/> (either next or prev update)
        T2->>List: CAS(prev.next, node, next) or <br/> CAS(head, node, next)
        
        T2->>List: Update remaining pointers
        Note right of List: Subsequent updates <br/> (non-linearization point)
    end
```

## Operation Details

### Push Front
```cpp
void push_front(Node* node) {
    assert(node != nullptr);
    
    // Initialize node
    node->m_next.store(nullptr, std::memory_order_relaxed);
    node->m_prev.store(nullptr, std::memory_order_relaxed);

    for (;;) {
        auto old_head = m_head.load(std::memory_order_acquire);
        node->m_next.store(old_head, std::memory_order_relaxed);
        
        // LINEARIZATION POINT
        if (m_head.compare_exchange_weak(old_head, node, 
                                       std::memory_order_release,
                                       std::memory_order_relaxed)) {
            if (old_head != nullptr) {
                old_head->m_prev.store(node, std::memory_order_release);
            } else {
                m_tail.store(node, std::memory_order_release);
            }
            return;
        }
    }
}
```

### Insert After
```cpp
bool insert_after(Node* target, Node* new_node) {
    assert(target != nullptr && new_node != nullptr);
    
    new_node->m_next.store(nullptr, std::memory_order_relaxed);
    new_node->m_prev.store(nullptr, std::memory_order_relaxed);
    
    for (;;) {
        auto next = target->m_next.load(std::memory_order_acquire);
        new_node->m_next.store(next, std::memory_order_relaxed);
        new_node->m_prev.store(target, std::memory_order_relaxed);
        
        // LINEARIZATION POINT
        if (target->m_next.compare_exchange_weak(next, new_node,
                                               std::memory_order_release,
                                               std::memory_order_relaxed)) {
            if (next != nullptr) {
                next->m_prev.store(new_node, std::memory_order_release);
            }
            return true;
        }
    }
}
```

### Remove
```cpp
void remove(Node* node) {
    assert(node != nullptr);
    
    for (;;) {
        auto prev = node->m_prev.load(std::memory_order_acquire);
        auto next = node->m_next.load(std::memory_order_acquire);
        
        if (prev != nullptr) {
            // LINEARIZATION POINT for non-head nodes
            if (prev->m_next.compare_exchange_strong(node, next,
                                                   std::memory_order_release)) {
                if (next != nullptr) {
                    next->m_prev.store(prev, std::memory_order_release);
                }
                return;
            }
        } else {
            // LINEARIZATION POINT for head node
            if (m_head.compare_exchange_strong(node, next,
                                             std::memory_order_release)) {
                if (next != nullptr) {
                    next->m_prev.store(nullptr, std::memory_order_release);
                }
                return;
            }
        }
    }
}
```

## Memory Ordering Requirements

| Operation | Load Order | Store Order | CAS Order |
|-----------|------------|-------------|-----------|
| push_front| acquire    | relaxed     | release   |
| insert_after| acquire   | relaxed     | release   |
| remove    | acquire    | release     | release   |
| find      | acquire    | N/A         | N/A       |

## Safety Properties

1. **No Lost Nodes**: All nodes are reachable from either the list or marked as removed
2. **No ABA Problems**: Handled through proper memory ordering and atomics
3. **Memory Safety**: All operations maintain list integrity under concurrent access
4. **Progress Guarantee**: Lock-free for all operations

## Progress Guarantees

1. **Lock-Freedom**: All operations are lock-free
2. **Wait-Freedom**: Traversal operations are wait-free
3. **Obstruction-Freedom**: Modifications succeed if run in isolation
