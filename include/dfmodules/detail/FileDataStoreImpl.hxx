
namespace dunedaq::dfmodules {

  template <FileHandleConcept FileHandleClass>
  inline std::string FileDataStoreImpl<FileHandleClass>::get_file_name(const daqdataformats::run_number_t run_number) const {

    std::ostringstream work_oss;
    work_oss << m_config_params->get_directory_path();
    if (work_oss.str().length() > 0) {
      work_oss << "/";
    }
    work_oss << m_operational_environment + "_" + m_config_params->get_filename_params()->get_file_type_prefix();
    if (work_oss.str().length() > 0) {
      work_oss << "_";
    }

    work_oss << m_config_params->get_filename_params()->get_run_number_prefix();
    work_oss << std::setw(m_config_params->get_filename_params()->get_digits_for_run_number()) << std::setfill('0')
             << run_number;
    work_oss << "_";

    work_oss << m_config_params->get_filename_params()->get_file_index_prefix();
    work_oss << std::setw(m_config_params->get_filename_params()->get_digits_for_file_index()) << std::setfill('0')
             << m_file_index;

    work_oss << "_" << m_writer_identifier;
    work_oss << "." << m_file_handle->get_file_name_extension();
    return work_oss.str();
  }

  template <FileHandleConcept FileHandleClass>
  inline size_t FileDataStoreImpl<FileHandleClass>::get_free_space(const std::string& the_path) const {
    struct statvfs vfs_results;
    int retval = statvfs(the_path.c_str(), &vfs_results);
    if (retval != 0) {
      return 0;
    }
    return vfs_results.f_bsize * vfs_results.f_bavail;
  }

  template <FileHandleConcept FileHandleClass>
  inline void FileDataStoreImpl<FileHandleClass>::throw_if_insufficient_space_for_object(const size_t obj_size,
											 const std::string& obj_name) const {

    size_t current_free_space = get_free_space(m_path);

    if (current_free_space < (m_free_space_safety_factor_for_write * obj_size)) {
      std::ostringstream msg_oss;
      msg_oss << "a safety factor of " << m_free_space_safety_factor_for_write << " times the " << obj_name << " size";
      InsufficientDiskSpace issue(ERS_HERE,
                                  get_name(),
                                  m_path,
                                  current_free_space,
                                  (m_free_space_safety_factor_for_write * obj_size),
                                  msg_oss.str());
      assert(m_file_handle);
      std::string msg =
        "writing a " + obj_name + " to file" + m_file_handle->get_file_name();
      throw RetryableDataStoreProblem(ERS_HERE, get_name(), msg, issue);
    }
  }
  
} // namespace dunedaq::dfmodules
