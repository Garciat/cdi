#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

////////////////////////

void panic(const char *message) {
    fprintf(stderr, "Panic: %s\n", message);
    exit(1);
}

////////////////////////

typedef struct {
    const unsigned char *data;
    size_t length;
} ArrayByte;

////////////////////////

#define MAX_MODULE_ENTRIES 16

typedef const char *ReflectionKey;

typedef const char *ReflectionValue;

const char REFLECTION_KEY_MODULE_NAME[] = "REFLECTION_KEY_MODULE_NAME";

const char REFLECTION_KEY_UNIT_NAME[] = "REFLECTION_KEY_UNIT_NAME";

const char REFLECTION_KEY_DEPENDENCY_TYPE[] = "REFLECTION_KEY_DEPENDENCY_TYPE";

const char REFLECTION_KEY_DEPENDENCY_END[] = "REFLECTION_KEY_DEPENDENCY_END";

typedef struct {
    ReflectionKey key;
    ReflectionValue value;
} ReflectionEntry;

typedef struct {
    ReflectionEntry name;
    bool initialized;
} UnitHeader;

typedef struct {
    ReflectionEntry name;
} ModuleHeader;

#define MODULE_INIT(module_name) \
    .__HEADER = { \
        .name = { \
            .key = REFLECTION_KEY_MODULE_NAME, \
            .value = #module_name, \
        }, \
    },

#define UNIT_DECL(unit_name) \
    UnitHeader __HEADER;

#define UNIT_INIT(unit_name) \
    .__HEADER = { \
        .name = { \
            .key = REFLECTION_KEY_UNIT_NAME, \
            .value = #unit_name, \
        }, \
        .initialized = false, \
    },

#define DEPENDENCY_DECL(type, name) \
    ReflectionEntry __DEP_##name; \
    struct type *name;

#define DEPENDENCY_DECL_END() \
    ReflectionEntry __DEP_END;

#define DEPENDENCY_INIT(type, name) \
    .__DEP_##name = { \
        .key = REFLECTION_KEY_DEPENDENCY_TYPE, \
        .value = #type, \
    },

#define DEPENDENCY_INIT_END() \
    .__DEP_END = { \
        .key = REFLECTION_KEY_DEPENDENCY_END, \
        .value = REFLECTION_KEY_DEPENDENCY_END, \
    },

typedef struct {
    ModuleHeader __HEADER;
    const void *entries[MAX_MODULE_ENTRIES];
} UnitModule;

typedef struct {
    const ReflectionEntry typeName;
    void *dependency;
} UnitDependencyHole;

typedef struct {
    UnitHeader header;
    UnitDependencyHole dependencies[];
} UnitReflection;

struct UnitList {
    UnitReflection *unit;
    const struct UnitList *next;
};

void UnitList_free(const struct UnitList *list) {
    while (list != NULL) {
        const struct UnitList *next = list->next;
        free((void *)list);
        list = next;
    }
}

struct UnitList* UnitList_push(struct UnitList *list, UnitReflection *unit) {
    struct UnitList *new = malloc(sizeof(*new));
    *new = (struct UnitList) {
        .unit = unit,
        .next = list,
    };
    return new;
}

struct UnitList* UnitModule_walk(const UnitModule *module, struct UnitList *list) {
    for (int i = 0; module->entries[i] != NULL; i++) {
        const void *entry = module->entries[i];

        const ReflectionEntry *info = (const ReflectionEntry *)entry;

        if (info->key == REFLECTION_KEY_UNIT_NAME) {
            UnitReflection *unit = (UnitReflection *)entry;
            list = UnitList_push(list, unit);
        } else if (info->key == REFLECTION_KEY_MODULE_NAME) {
            list = UnitModule_walk(entry, list);
        } else {
            panic("Unknown reflection key");
        }
    }
    return list;
}

UnitReflection* UnitList_lookup(const struct UnitList list[static 1], const char *name) {
    for (const struct UnitList *elem = list; elem != NULL; elem = elem->next) {
        if (elem->unit->header.name.value == name) {
            return elem->unit;
        }
    }
    return NULL;
}

void Unit_init(struct UnitList *units, UnitReflection *target) {
    if (target->header.initialized) {
        return;
    }

    printf("[%s] Initializing\n", target->header.name.value);

    for (UnitDependencyHole *hole = target->dependencies; hole->typeName.key != REFLECTION_KEY_DEPENDENCY_END; ++hole) {
        if (hole->typeName.key == REFLECTION_KEY_DEPENDENCY_TYPE) {
            UnitReflection *unit = UnitList_lookup(units, hole->typeName.value);
            if (unit == NULL) {
                printf("[%s] Not found: %s\n", target->header.name.value, hole->typeName.value);
                panic("Dependency not found");
            }
            Unit_init(units, unit);
            hole->dependency = (void *)unit;
            printf("[%s] Resolved: %s\n", target->header.name.value, hole->typeName.value);
        } else {
            panic("Unknown reflection key");
        }
    }

    target->header.initialized = true;
}

void UnitModule_init(const UnitModule *root) {
    struct UnitList *units = UnitModule_walk(root, NULL);

    for (const struct UnitList *elem = units; elem != NULL; elem = elem->next) {
        Unit_init(units, elem->unit);
    }

    UnitList_free(units);
}

////////////////////////

typedef struct {
    const char *data;
    size_t length;
} String;

////////////////////////

// INTERFACE

typedef struct {
    String value;
} Base64String;

struct Base64Encoder;

Base64String Base64Encoder_encode(ArrayByte bytes);

struct Base64Encoder {
    UNIT_DECL(Base64Encoder)
    DEPENDENCY_DECL_END()
    Base64String (*encode)(ArrayByte bytes);
};

struct Base64Encoder Unit_Base64Encoder;

// IMPLEMENTATION

struct Base64Encoder Unit_Base64Encoder = {
    UNIT_INIT(Base64Encoder)
    DEPENDENCY_INIT_END()
    .encode = Base64Encoder_encode,
};

Base64String Base64Encoder_encode(ArrayByte _) {
    return (Base64String) {
        .value = (String) {
            .data = "base64",
            .length = 6
        }
    };
}

////////////////////////

// INTERFACE

typedef struct {
    ArrayByte bytes;
} PasswordSalt;

struct SaltSerializer;
Base64String SaltSerializer_serialize(PasswordSalt salt);

struct SaltSerializer {
    UNIT_DECL(SaltSerializer)
    DEPENDENCY_DECL(Base64Encoder, _encoder)
    DEPENDENCY_DECL_END()
    Base64String (*serialize)(PasswordSalt salt);
};

struct SaltSerializer Unit_SaltSerializer;

// IMPLEMENTATION

struct SaltSerializer Unit_SaltSerializer = {
    UNIT_INIT(SaltSerializer)
    DEPENDENCY_INIT(Base64Encoder, _encoder)
    DEPENDENCY_INIT_END()
    .serialize = SaltSerializer_serialize,
};

Base64String SaltSerializer_serialize(PasswordSalt salt) {
    return Unit_SaltSerializer._encoder->encode(salt.bytes);
}

////////////////////////

// INTERFACE

UnitModule UnitModule_Passwords;

// IMPLEMENTATION

UnitModule UnitModule_Passwords = {
    MODULE_INIT(Passwords)
    .entries = {
        &Unit_Base64Encoder,
        &Unit_SaltSerializer,
        NULL
    }
};

////////////////////////

// INTERFACE

UnitModule UnitModule_Main;

// IMPLEMENTATION

UnitModule UnitModule_Main = {
    MODULE_INIT(Main)
    .entries = {
        &UnitModule_Passwords,
        NULL
    }
};

////////////////////////

int run_app() {
    printf("Running App\n");

    UnitModule_init(&UnitModule_Main);

    Base64String s = SaltSerializer_serialize((PasswordSalt) {
        .bytes = (ArrayByte) {
            .data = (const unsigned char *)"password",
            .length = 8
        }
    });

    printf("Result: %.*s\n", (int)s.value.length, s.value.data);

    return 0;
}

// ========================

Base64String mock_encode(ArrayByte _) {
    return (Base64String) {
        .value = (String) {
            .data = "test",
            .length = 4
        }
    };
}

void Test_SaltSerializer_serialize() {
    printf("[Test_SaltSerializer_serialize] Start\n");

    Unit_SaltSerializer._encoder = &(struct Base64Encoder){
        .encode = mock_encode,
    };

    Base64String s = SaltSerializer_serialize((PasswordSalt) {
        .bytes = (ArrayByte) {
            .data = (const unsigned char *)"password",
            .length = 8
        }
    });

    if (s.value.length != 4 || memcmp(s.value.data, "test", 4) != 0) {
        panic("Test failed");
    }

    printf("[Test_SaltSerializer_serialize] OK\n");
}

int run_tests() {
    printf("Running Tests\n");

    Test_SaltSerializer_serialize();

    return 0;
}

// ========================

int main(int argc, char *argv[]) {
    if (argc == 2 && strcmp(argv[1], "test") == 0) {
        return run_tests();
    } else {
        return run_app();
    }
}
