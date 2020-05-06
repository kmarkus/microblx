Module lfds_cyclic
------------------

Block lfds_buffers/cyclic
^^^^^^^^^^^^^^^^^^^^^^^^^

| **Type**:       iblock
| **Attributes**: 
| **Meta-data**:  { doc='High performance scalable, lock-free cyclic, buffered in process communication  description=[[		 This version is stongly typed and should be preferred                This microblx iblock is based on based on liblfds                ringbuffer (v0.6.1.1) (www.liblfds.org)]],  version=0.01,  hard_real_time=true,}
| **License**:    BSD-3-Clause


Configs
"""""""

.. csv-table::
   :header: "name", "type", "doc"

   type_name, ``char``, "name of registered microblx type to transport"
   data_len, ``uint32_t``, "array length (multiplier) of data (default: 1)"
   buffer_len, ``uint32_t``, "max number of data elements the buffer shall hold"
   loglevel_overruns, ``int``, "loglevel for reporting overflows (default: NOTICE, -1 to disable)"



Ports
"""""

.. csv-table::
   :header: "name", "out type", "out len", "in type", "in len", "doc"

   overruns, ``unsigned long``, 1, , , "Number of buffer overruns. Value is output only upon change."



