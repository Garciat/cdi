#include <assert.h>
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
} ByteArray;

bool ByteArray_equal_str(ByteArray a, const char *b) {
    return a.length == strlen(b) && memcmp(a.data, b, a.length) == 0;
}

////////////////////////

#define MAX_MODULE_IMPORTS 16
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

struct UnitModule;

typedef struct UnitModule {
    ModuleHeader __HEADER;
    const struct UnitModule *imports[MAX_MODULE_IMPORTS];
    const UnitHeader *units[MAX_MODULE_ENTRIES];
} UnitModule;

#define UnitModule_foreach_import(module, elem) \
    for (const struct UnitModule *elem = module->imports; elem != NULL; ++elem)

#define UnitModule_foreach_unit(module, elem) \
    for (const UnitHeader *elem = module->units; elem != NULL; ++elem)

typedef struct {
    const ReflectionEntry name;
    void *slot;
} UnitDependencyDecl;

void UnitDependencyDecl_set_slot(UnitDependencyDecl *decl, void *slot) {
    decl->slot = slot;
}

const char *UnitDependencyDecl_name(const UnitDependencyDecl *decl) {
    return decl->name.value;
}

typedef struct {
    UnitHeader header;
    UnitDependencyDecl dependencies[];
} UnitReflection;

#define UnitReflection_foreach_dependency(unit, decl) \
    for (UnitDependencyDecl *decl = unit->dependencies; decl->name.key != REFLECTION_KEY_DEPENDENCY_END; ++decl)

bool UnitReflection_is_initialized(const UnitReflection *unit) {
    return unit->header.initialized;
}

void UnitReflection_set_initialized(UnitReflection *unit) {
    unit->header.initialized = true;
}

const char *UnitReflection_name(const UnitReflection *unit) {
    return unit->header.name.value;
}

bool UnitReflection_has_name(const UnitReflection *unit, const char *name) {
    return strcmp(unit->header.name.value, name) == 0;
}

#define MODULE_INIT(module_name) \
    .__HEADER = { \
        .name = { \
            .key = REFLECTION_KEY_MODULE_NAME, \
            .value = #module_name, \
        }, \
    },

#define UNIT_ADD(unit_obj) \
    &unit_obj.__HEADER

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

// Fields MUST align with `UnitDependencyDecl`
#define DEPENDENCY_DECL(type, name) \
    ReflectionEntry __DEP_##name; \
    struct type *name;

#define DEPENDENCY_INIT(type, name) \
    .__DEP_##name = { \
        .key = REFLECTION_KEY_DEPENDENCY_TYPE, \
        .value = #type, \
    }, \
    .name = NULL,

#define DEPENDENCY_DECL_END() \
    ReflectionEntry __DEP_END;

#define DEPENDENCY_INIT_END() \
    .__DEP_END = { \
        .key = REFLECTION_KEY_DEPENDENCY_END, \
        .value = REFLECTION_KEY_DEPENDENCY_END, \
    },

struct UnitList {
    UnitReflection *unit;
    const struct UnitList *next;
};

#define UnitList_foreach(list, elem) \
    for (const struct UnitList *elem = list; elem != NULL; elem = elem->next)

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
    for (int i = 0; module->units[i] != NULL; i++) {
        UnitReflection *unit = (UnitReflection *)module->units[i]; // upcast
        list = UnitList_push(list, unit);
    }

    for (int i = 0; module->imports[i] != NULL; i++) {
        const struct UnitModule *import = module->imports[i];
        list = UnitModule_walk(import, list);
    }

    return list;
}

UnitReflection* UnitList_lookup(const struct UnitList list[static 1], const char *name) {
    UnitList_foreach(list, elem) {
        if (UnitReflection_has_name(elem->unit, name)) {
            return elem->unit;
        }
    }
    return NULL;
}

void Unit_init(struct UnitList *units, UnitReflection *target) {
    if (UnitReflection_is_initialized(target)) {
        return;
    }

    printf("[%s] Initializing\n", UnitReflection_name(target));

    UnitReflection_foreach_dependency(target, decl) {
        assert(decl->name.key == REFLECTION_KEY_DEPENDENCY_TYPE);

        UnitReflection *unit = UnitList_lookup(units, UnitDependencyDecl_name(decl));
        if (unit == NULL) {
            printf("[%s] Not found: %s\n", UnitReflection_name(target), UnitDependencyDecl_name(decl));
            panic("Dependency not found");
        }

        UnitDependencyDecl_set_slot(decl, unit);

        printf("[%s] Resolved: %s\n", UnitReflection_name(target), UnitDependencyDecl_name(decl));
    }

    UnitReflection_set_initialized(target);
}

void UnitModule_init(const UnitModule *root) {
    struct UnitList *units = UnitModule_walk(root, NULL);

    UnitList_foreach(units, elem) {
        Unit_init(units, elem->unit);
    }

    UnitList_free(units);
}

////////////////////////

typedef struct {
    const char *data;
    size_t length;
} String;

bool String_equal_str(String a, const char *b) {
    return a.length == strlen(b) && memcmp(a.data, b, a.length) == 0;
}

////////////////////////

// INTERFACE

typedef struct {
    String value;
} Base64String;

struct Base64Encoder;

Base64String Base64Encoder_encode(ByteArray bytes);

struct Base64Encoder {
    UNIT_DECL(Base64Encoder)
    DEPENDENCY_DECL_END()
    Base64String (*encode)(ByteArray bytes);
};

struct Base64Encoder Unit_Base64Encoder;

// IMPLEMENTATION

struct Base64Encoder Unit_Base64Encoder = {
    UNIT_INIT(Base64Encoder)
    DEPENDENCY_INIT_END()
    .encode = Base64Encoder_encode,
};

Base64String Base64Encoder_encode(ByteArray _) {
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
    ByteArray bytes;
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
    .units = {
        UNIT_ADD(Unit_Base64Encoder),
        UNIT_ADD(Unit_SaltSerializer),
    }
};

////////////////////////

// INTERFACE

UnitModule UnitModule_Main;

// IMPLEMENTATION

UnitModule UnitModule_Main = {
    MODULE_INIT(Main)
    .imports = {
        &UnitModule_Passwords,
    },
};

// ========================

#define TEST_ASSERT(condition) \
    if (!(condition)) { \
        panic("Assertion failed: " #condition); \
    }

#define RUN_TEST(test_name) \
    printf("[Test] " #test_name " ...\n"); \
    test_name(); \
    printf("[Test] " #test_name " OK\n");

// ========================

static ByteArray mock_encode_arg;

static Base64String mock_encode(ByteArray bytes) {
    mock_encode_arg = bytes;
    return (Base64String) {
        .value = (String) {
            .data = "test",
            .length = 4
        }
    };
}

void Test_SaltSerializer_serialize() {
    Unit_SaltSerializer._encoder = &(struct Base64Encoder){
        .encode = mock_encode,
    };

    Base64String s = SaltSerializer_serialize((PasswordSalt) {
        .bytes = (ByteArray) {
            .data = (const unsigned char *)"password",
            .length = 8
        }
    });

    TEST_ASSERT(ByteArray_equal_str(mock_encode_arg, "password"));

    TEST_ASSERT(String_equal_str(s.value, "test"));
}

int run_tests() {
    printf("Running Tests\n");

    RUN_TEST(Test_SaltSerializer_serialize);

    return 0;
}

////////////////////////

int run_app() {
    printf("Running App\n");

    UnitModule_init(&UnitModule_Main);

    Base64String s = SaltSerializer_serialize((PasswordSalt) {
        .bytes = (ByteArray) {
            .data = (const unsigned char *)"password",
            .length = 8
        }
    });

    printf("Result: %.*s\n", (int)s.value.length, s.value.data);

    return 0;
}

////////////////////////

int main(int argc, char *argv[]) {
    if (argc == 2 && strcmp(argv[1], "test") == 0) {
        return run_tests();
    } else {
        return run_app();
    }
}
