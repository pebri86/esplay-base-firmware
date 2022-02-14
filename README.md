# esplay-base-firmware
Bootloader or sdcard flasher for ESPlay HW ESP32 based device.
The code based on odroid go base firmware, modified using hardware drivers to be compatible with [esplay-hardware].

Compiling
---------
Use esp-idf official release version 4.4

add this patch to esp-idf

```
@file components/spi_flash/include/esp_partition.h

void esp_partition_reload_table();
```

and 

```
@file components/spi_flash/partition.c

void esp_partition_reload_table()
{
    if (!SLIST_EMPTY(&s_partition_list))
    {
        _lock_acquire(&s_partition_list_lock);

        // Remove all entries
        while(!SLIST_EMPTY(&s_partition_list))
        {
            partition_list_item_t* item = SLIST_FIRST(&s_partition_list);
            SLIST_REMOVE_HEAD(&s_partition_list, next);

            free(item);
        }

        _lock_release(&s_partition_list_lock);
    }
}

```
[esplay-hardware]: https://github.com/pebri86/esplay-hardware