#include "event_list.h"

namespace nano_edr {

// Узлы вернутся на любом выходе из области видимости, и при исключении тоже.
EventList::~EventList() {
    ListClear(this);
}

void ListPushBack(EventList* list, const Event* event) {
    if (list->capacity > 0 && list->size >= list->capacity) {
        ListPopFront(list);
    }

    auto* node = new EventNode{*event, nullptr};
    if (list->tail == nullptr) {
        list->head = node;
    } else {
        list->tail->next = node;
    }
    list->tail = node;
    ++list->size;
}

// Пустой список — не ошибка, просто нечего убирать.
void ListPopFront(EventList* list) {
    if (list->head == nullptr) {
        return;
    }

    EventNode* old_head = list->head;
    list->head = old_head->next;
    delete old_head;
    --list->size;

    // Иначе tail останется висячим.
    if (list->head == nullptr) {
        list->tail = nullptr;
    }
}

// capacity — настройка, её не сбрасываем.
void ListClear(EventList* list) {
    while (list->head != nullptr) {
        ListPopFront(list);
    }
}

}  // namespace nano_edr
