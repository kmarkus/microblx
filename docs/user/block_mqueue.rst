Module mqueue
-------------

Block mqueue
^^^^^^^^^^^^

| **Type**:       iblock
| **Attributes**: 
| **Meta-data**:  { doc='POSIX mqueue interaction',  realtime=true,}
| **License**:    BSD-3-Clause


Configs
"""""""

.. csv-table::
   :header: "name", "type", "doc"

   mq_id, ``char``, "mqueue base id"
   type_name, ``char``, "name of registered microblx type to transport"
   data_len, ``long``, "array length (multiplier) of data (default: 1)"
   buffer_len, ``long``, "max number of data elements the buffer shall hold"
   blocking, ``uint32_t``, "enable blocking mode (def: 0)"





