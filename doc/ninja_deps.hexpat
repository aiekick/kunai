#pragma endian little
#pragma pattern_limit 1000000

struct Header {
    char signature[12];   // "# ninjadeps\n"
    u32 version;          // 3 ou 4
};

struct PathRecord {
    u32 size_and_type;    
    // Taille sans le header
    u32 payload_size = size_and_type & 0x7FFFFFFF;
    u32 string_size = payload_size - 4;  // -4 pour le checksum final    
    char path[string_size];
    u32 checksum;         // = ~ID (complément binaire de l'ID)
};

struct DepsRecordV4 {
    u32 size_and_type;    
    u32 payload_size = size_and_type & 0x7FFFFFFF;    
    u32 output_id;        // ID du target (référence un PathRecord)
    u64 mtime;            // Timestamp modification (64-bit)    
    // Calcul du nombre de deps: (taille - sizeof(output_id) - sizeof(mtime)) / 4
    u32 num_deps = (payload_size - 12) / 4;
    u32 dep_ids[num_deps];
};

struct DepsRecordV3 {
    u32 size_and_type;    
    u32 payload_size = size_and_type & 0x7FFFFFFF;    
    u32 output_id;
    u32 mtime;            // Timestamp modification (32-bit)    
    u32 num_deps = (payload_size - 8) / 4;
    u32 dep_ids[num_deps];
};

struct Record {
    // Peek le header pour déterminer le type
    u32 peek @ $ [[hidden, no_unique_address]];    
    bool is_deps = (peek & 0x80000000) != 0 [[hidden]];
    u32 payload_size = peek & 0x7FFFFFFF [[hidden]];    
    if (payload_size == 0) {
        u32 empty;
    } else if (!is_deps) {
        PathRecord path [[inline]];
    } else {
        // Version 4 par défaut (mtime 64-bit)
        DepsRecordV4 deps [[inline]];
    }
};

struct NinjaDeps {
    Header header [[color("00AA00"), name("Header")]];
    Record records[while($ < sizeof($))] [[color("0088FF"), name("Records")]];
};

NinjaDeps file @ 0x0;
