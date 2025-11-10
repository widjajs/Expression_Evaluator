#include "../includes/object.h"
#include "../includes/memory.h"
#include "../includes/value.h"
#include "../includes/vm.h"

static Object_t *allocate_object(size_t size, ObjectType_t type) {
    Object_t *new_object = (Object_t *)(malloc(size));
    new_object->type = type;
    new_object->next = vm.objects;
    vm.objects = new_object;
    return new_object;
}

ObjectStr_t *allocate_str(char *chars, int length) {
    ObjectStr_t *str = ALLOCATE_OBJ(ObjectStr_t, OBJ_STR);
    str->length = length;
    str->chars = chars;
    return str;
}

ObjectStr_t *copy_str(const char *chars, int length) {
    printf("copy_str called: chars=%p, length=%d\n", chars, length);
    char *new_str = ALLOCATE(char, length + 1);
    printf("Allocated new_str at: %p\n", new_str);
    memcpy(new_str, chars, length);
    new_str[length] = '\0';

    return allocate_str(new_str, length);
}
