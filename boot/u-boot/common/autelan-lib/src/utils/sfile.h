#ifndef __SFILE_H_CAF3737A24BFD00978F62CEB481C30B0__
#define __SFILE_H_CAF3737A24BFD00978F62CEB481C30B0__
#ifdef __APP__
/******************************************************************************/
/* simpile file api */
/******************************************************************************/

static inline void 
os_fd_set_cloexec(int fd)
{
#ifdef FD_CLOEXEC
	fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);
#endif
}

static inline bool 
os_sfscan_match_all(char *context)
{   /*
    * just "*"
    */
    return '*'==context[0] && 0==context[1];
}

static inline bool
os_sfcan_is_dir_self(char *filename/* not include path */)
{
    return '.'==filename[0] && 0==filename[1];
}

static inline bool
os_sfcan_is_dir_father(char *filename/* not include path */)
{
    return '.'==filename[0] && '.'==filename[1] && 0==filename[2];
}

/*
* @filename: not include path
*/
typedef multi_value_t os_sfscan_line_handle_t(char *filename, char *line, void *control);
typedef multi_value_t os_sfscan_file_handle_t(char *path, char *filename, 
                            os_sfscan_line_handle_t *line_handle, void *control);
typedef bool os_sfscan_file_filter_t(char *path, char *filename, void *control);

static inline multi_value_t 
os_sfscan_file_handle
(
    char *path,
    char *filename,
    os_sfscan_line_handle_t *line_handle,
    void *control
)
{
    FILE *stream = NULL;
    char line[1+OS_LINE_LEN];
    multi_value_u mv;

    if (NULL==path) {
        return os_assert_value(mv2_GO(-EINVAL1));
    }
    else if (NULL==filename) {
        return os_assert_value(mv2_GO(-EINVAL2));
    }
    else if (NULL==line_handle) {
        return os_assert_value(mv2_GO(-EINVAL3));
    }
    
    stream = os_v_fopen("r", "%s/%s", path, filename);
    if (NULL==stream) {
        mv2_result(mv) = errno;

        goto error;
    }

    while(false==feof(stream)) {
        os_objzero(&line); /* os_objzero(&array) */
        fgets(line, OS_LINE_LEN, stream);
        
        /*
        * strim left/right blank
        */
        __string_strim_both_ends(line, NULL);
        
        /*
        * replace blank("\t \r\n") to ' '
        */
        __string_replace(line, NULL, ' ');
        
        /*
        * reduce ' '
        */
        __string_reduce(line, NULL);
        
        /*
        * skip blank line
        */
        if (__is_blank_line(line)) {
            continue;
        }
        
        /*
        * skip notes line
        */
        if (__is_notes_line_deft(line)) {
            continue;
        }
        
        mv.value = (*line_handle)(filename, line, control);
        if (mv2_is_break(mv)) {
            goto error;
        }
    }

    mv.value = mv2_OK;
error:
    if (stream) {
        fclose(stream);
    }
    
    return mv.value;
}

/*
* @filefilter: if return true, ignore file
*/
static inline int 
__os_sfscan_dir
(
    char *path, 
    bool recur,
    os_sfscan_file_filter_t *file_filter,
    os_sfscan_file_handle_t *file_handle,
    os_sfscan_line_handle_t *line_handle,
    void *control
)
{
    DIR *dir = NULL;
    struct dirent *file = NULL;
    struct stat st;
    multi_value_u mv;
    int err = 0;

    if (NULL==path) {
        return os_assert_value(-EINVAL1);
    }
    else if (NULL==file_handle) {
        return os_assert_value(-EINVAL2);
    }
    
    dir = opendir(path);
    if (NULL == dir) {
        err = errno;
        
        goto error;
    }
    
    while (NULL != (file=readdir(dir))) {
        char *filename = file->d_name; /* just name, not include path */
        
        /*
        * skip . and ..
        */
        if (os_sfcan_is_dir_self(filename) || os_sfcan_is_dir_father(filename)) {
            continue;
        }
        
        /*
        * dir
        */
        if (stat(filename, &st) >= 0 && S_ISDIR(st.st_mode)) {
            if (recur) {
                err = __os_sfscan_dir(path, recur, file_filter, file_handle, line_handle, control);
                if (err<0) {
                    goto error;
                }
            } else {
                continue;
            }
        }
        
        /*
        * file filter
        */
        if (file_filter && (*file_filter)(path, filename, control)) {
            continue;
        }
        
        /*
        * file handle
        */
        mv.value = (*file_handle)(path, filename, line_handle, control);
        if (mv2_is_break(mv)) {
            err = mv2_result(mv);

            goto error;
        }
    }
    
error:
    if (dir) {
        closedir(dir);
    }

    return err;
}

static inline int 
os_sfscan_dir
(
    char *path, 
    bool recur,
    os_sfscan_file_filter_t *file_filter,
    os_sfscan_line_handle_t *line_handle,
    void *control
)
{
    return __os_sfscan_dir(path, recur, file_filter, os_sfscan_file_handle, line_handle, control);
}
/******************************************************************************/
#define __os_sgetx(prefix, stream, vfmt, pv) ({ \
    int err = 0;                            \
                                                \
    if (NULL==(stream)) {                       \
        err = errno;                            \
    } else if (1!=fscanf(stream, vfmt, pv)) {   \
        err = -EFORMAT;                         \
    }                                           \
                                                \
    if (stream) {                               \
        prefix##close(stream);                  \
    }                                           \
                                                \
    err;                                        \
})

#define __os_sgets(prefix, stream, line, space) ({ \
    int err = 0;                            \
                                                \
    if (NULL==(line)) {                         \
        err = -EINVAL9;                         \
    } else if ((space)<=0) {                    \
        err = -EINVAL8;                         \
    } else if (NULL==(stream)) {                \
        err = errno;                            \
    } else if (NULL==fgets(line, space, stream)) { \
        err = errno;                            \
    } else {                                    \
        __string_strim_both_ends(line, NULL);   \
    }                                           \
                                                \
    if (stream) {                               \
        prefix##close(stream);                  \
    }                                           \
                                                \
    err;                                        \
})

#define os_sgetx(prefix, vfmt, pv, filename) ({ \
    FILE *stream = prefix##open("r", filename); \
    int err = __os_sgetx(prefix, stream, vfmt, pv); \
    err; \
})
#define os_sgets(prefix, line, space, filename) ({ \
    FILE *stream = prefix##open("r", filename); \
    int err = __os_sgets(prefix, stream, line, space); \
    err; \
})
#define os_sgeti(prefix, pi, filename)          os_sgetx(prefix, "%u", pi, filename)
#define os_sgetll(prefix, pll, filename)        os_sgetx(prefix, "%llu", pll, filename)


#define os_v_sgetx(prefix, vfmt, pv, fmt, args...) ({ \
    FILE *stream = os_v_##prefix##open("r", fmt, ##args); \
    int err = __os_sgetx(prefix, stream, vfmt, pv); \
    err; \
})
#define os_v_sgets(prefix, line, space, fmt, args...) ({ \
    FILE *stream = os_v_##prefix##open("r", fmt, ##args); \
    int err = __os_sgets(prefix, stream, line, space); \
    err; \
})
#define os_v_sgeti(prefix, pi, fmt, args...)    os_v_sgetx(prefix, "%u", pi, fmt, ##args)
#define os_v_sgetll(prefix, pll, fmt, args...)  os_v_sgetx(prefix, "%u", pll, fmt, ##args)


/*
* get (string/int/long long int) from file
*/
#define os_sfgets(line, space, filename)        os_sgets(f, line, space, filename)
#define os_sfgeti(pi, filename)                 os_sgeti(f, pi, filename)
#define os_sfgetll(pll, filename)               os_sgetll(f, pll, filename)

#define os_v_sfgets(line, space, fmt, args...)  os_v_sgets(f, line, space, fmt, ##args)
#define os_v_sfgeti(pi, fmt, args...)           os_v_sgeti(f, pi, fmt, ##args)
#define os_v_sfgetll(pll, fmt, args...)         os_v_sgeti(f, pll, fmt, ##args)

/*
* get (string/int/long long int) from pipe
*/
#define os_spgets(line, space, filename)        os_sgets(p, line, space, filename)
#define os_spgeti(pi, filename)                 os_sgeti(p, pi, filename)
#define os_spgetll(pll, filename)               os_sgetll(p, pll, filename)

#define os_v_spgets(line, space, fmt, args...)  os_v_sgets(p, line, space, fmt, ##args)
#define os_v_spgeti(pi, fmt, args...)           os_v_sgeti(p, pi, fmt, ##args)
#define os_v_spgetll(pll, fmt, args...)         os_v_sgeti(p, pll, fmt, ##args)


#define __os_ssetx(prefix, stream, vfmt, v) ({ \
    int err = 0;                            \
                                                \
    if (NULL==(stream)) {                       \
        err = errno;                            \
    } else {                                    \
        err = fprintf(stream, vfmt, v);         \
    }                                           \
                                                \
    if (stream) {                               \
        prefix##close(stream);                  \
    }                                           \
                                                \
    err;                                        \
})

#define os_ssetx(prefix, vfmt, pv, filename) ({ \
    FILE *stream = prefix##open(filename, "w"); \
    int err = __os_ssetx(prefix, stream, vfmt, pv); \
    err; \
})
#define os_v_ssetx(prefix, vfmt, pv, fmt, args...) ({ \
    FILE *stream = os_v_##prefix##open("w", fmt, ##args); \
    int err = __os_ssetx(prefix, stream, vfmt, pv); \
    err; \
})
    
#define os_ssets(prefix, string, filename)          os_ssetx(prefix, "%s", string, filename)
#define os_sseti(prefix, vi, filename)              os_ssetx(prefix, "%u", vi, filename)
#define os_ssetll(prefix, vll, filename)            os_ssetx(prefix, "%llu", vll, filename)

#define os_v_ssets(prefix, string, fmt, args...)    os_v_ssetx(prefix, "%s", string, fmt, ##args)
#define os_v_sseti(prefix, vi, fmt, args...)        os_v_ssetx(prefix, "%u", vi, fmt, ##args)
#define os_v_ssetll(prefix, vll, fmt, args...)      os_v_ssetx(prefix, "%llu", vll, fmt, ##args)

/*
* set (string/int/long long int) to file
*/
#define os_sfsets(string, filename)                 os_ssets(f, string, filename)
#define os_sfseti(vi, filename)                     os_sseti(f, vi, filename)
#define os_sfsetll(vll, filename)                   os_ssetll(f, vll, filename)

#define os_v_sfsets(string, fmt, args...)           os_v_ssets(f, string, fmt, ##args)
#define os_v_sfseti(vi, fmt, args...)               os_v_sseti(f, vi, fmt, ##args)
#define os_v_sfsetll(vll, fmt, args...)             os_v_ssetll(f, vll, fmt, ##args)

/*
* get file size by full filename(include path)
*/
static inline int
os_sfsize(char *filename)
{
    struct stat st;
    int err;
    
    err = stat(filename, &st);
    if (err<0) {
        return errno;
    } else {
        return st.st_size;
    }
}

/*
* get file size
*/
#define os_v_sfsize(fmt, args...) ({   \
    int size;                           \
    char buf[1+OS_LINE_LEN] = {0};      \
                                        \
    os_saprintf(buf, fmt, ##args);      \
    size = os_sfsize(buf);              \
                                        \
    size;                               \
})
/******************************************************************************/
#endif /* __KERNEL__ */
#endif /* __SFILE_H_CAF3737A24BFD00978F62CEB481C30B0__ */
