#include <Python.h>

PyObject *InitializeError_ = nullptr,
				 *FinalizeError_ = nullptr,
				 *PerformPartitionError_ = nullptr,
				 *RunGenerateGraphError_ = nullptr,
				 *RunExecuteGraphError_ = nullptr,
				 *RunSkipDataError_ = nullptr,
		 		 *ParseSchemaError_ = nullptr,
				 *RegisterFileSystemHDFSError_ = nullptr,
				 *RegisterFileSystemGCSError_ = nullptr,
		 		 *RegisterFileSystemS3Error_ = nullptr,
				 *RegisterFileSystemLocalError_ = nullptr,
         *InferFolderPartitionMetadataError_ = nullptr,
				 *GetProductDetailsError_ = nullptr,
				 *GetFreeMemoryError_ = nullptr,
				 *ResetMaxMemoryUsedError_ = nullptr,
				 *GetMaxMemoryUsedError_ = nullptr ;


// PyErr_SetString
