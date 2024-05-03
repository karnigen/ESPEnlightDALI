idf_component_register(SRCS
        "dali.c"
        "dali_hw.c"
        "uart_parser.c"
        "uart_queue.c"
    INCLUDE_DIRS .
    REQUIRES driver
    )

