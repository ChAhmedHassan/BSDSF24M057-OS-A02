# OS Assignment 02 Report: Re-engineering ls

**Name:** Ch Ahmed Hassan
**Roll No:** BSDSF24M057

---

## Feature 2: Long Listing Format

**Q1. Difference between stat() and lstat(), and when is lstat() more appropriate?**

**Ans:** `stat()` follows symbolic links and returns information about the file the link points to. `lstat()` does not follow the link — it returns information about the link itself. `ls` should use `lstat()` so that when listing a symbolic link, it shows the link's own properties (like marking it as a link with 'l' and showing its own size), not silently resolving it to the target file.

**Q2. How do bitwise operators and macros extract info from st_mode?**

**Ans:** `st_mode` is a single integer where different bits represent different things: some bits encode the file type (regular file, directory, symlink), and others encode the nine permission bits (read/write/execute for owner, group, others). Macros like `S_ISDIR(mode)` test the file-type bits using a mask and comparison internally. Macros like `S_IRUSR` are single-bit masks; using `mode & S_IRUSR` performs a bitwise AND, which is non-zero only if that specific bit is set in `mode`, telling us whether that one permission is granted.



## Feature 3: Column Display

**Q1. Logic for "down then across" and why a single loop isn't enough?**

**Ans:** A single loop through the array in order would print filenames left-to-right (across), not down-then-across. To fill column-by-column, I first calculate the number of rows needed, then for each row and column position I compute the array index as `col * num_rows + row`. This jumps forward by num_rows each time we move to a new column, so column 0 holds the first num_rows filenames top-to-bottom, column 1 holds the next num_rows, and so on.

**Q2. Purpose of ioctl, and limitations of a fixed 80-column fallback?**

**Ans:** `ioctl()` with `TIOCGWINSZ` asks the terminal for its actual current width in characters, so the column count adapts to how wide the user's terminal really is. If I only used a fixed 80-column fallback, the output would look wrong on any terminal that isn't exactly 80 columns wide — too few columns on a wide terminal (wasting space) or text wrapping awkwardly on a narrower one.



## Feature 4: Horizontal Display (-x)

**Q1. Compare complexity of down-then-across vs across logic.**

**Ans:** The "across" (horizontal) layout is simpler: I just track the current horizontal position and wrap to a new line once the next name would overflow the terminal width — a single left-to-right pass. The "down then across" layout needs more pre-calculation: I must first compute the total number of rows the data will need, then use the index formula `col * num_rows + row` to know which filename belongs in each screen position, since the array itself is stored in row-major order but needs to be displayed column-major.

**Q2. How did you manage the different display modes?**

**Ans:** I used an enum (`DisplayMode`) with three values: default, long, and horizontal. `getopt()` sets this variable based on which flag (`-l` or `-x`) was passed. After reading all filenames into the array, a single `switch` statement on this enum decides which display function to call.



## Feature 5: Alphabetical Sort

**Q1. Why read all entries into memory before sorting? Drawbacks for huge directories?**

**Ans:** Sorting requires comparing items against each other, so you need the complete set in memory before you can determine any item's final position — you can't sort a stream you're still reading. The drawback is memory usage: for a directory with millions of files, holding every filename (plus pointers) in RAM at once could use a large amount of memory and take noticeably longer before any output appears, compared to a streaming approach.

**Q2. Purpose and signature of the qsort comparison function?**

**Ans:** `qsort()` is generic and works on any data type, so it has no built-in way to compare two elements of whatever array you give it. The comparison function tells it how: it receives two `const void *` pointers (generic, type-less pointers so qsort can call it on any data) pointing at two elements in the array, and must return negative, zero, or positive depending on their order. Inside, I cast the `void *` back to `const char **` (since my array holds `char *` strings) and use `strcmp` to compare the actual string contents.



## Feature 6: Colorized Output

**Q1. How do ANSI escape codes produce color? Code for green text?**

**Ans:** ANSI escape codes are special character sequences starting with the ESC character (\033) that the terminal interprets as formatting instructions instead of visible text. To print green text: `printf("\033[0;32m%s\033[0m", text);` — `\033[0;32m` switches to green, and `\033[0m` resets back to the terminal's default color afterward.

**Q2. Which bits in st_mode determine if a file is executable?**

**Ans:** Three separate bits: `S_IXUSR` (owner execute), `S_IXGRP` (group execute), `S_IXOTH` (others execute). I check `st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)` — if any of those bits is set, the file is executable by someone, and I color it green.



## Feature 7: Recursive Listing (-R)

**Q1. What is a base case, and what is it here?**

**Ans:** A base case is the condition that stops a recursive function from calling itself again. Here, `do_ls` only makes a recursive call when an entry is a directory (checked with `S_ISDIR(st.st_mode)`). Regular files never trigger another call, so every path through the directory tree eventually reaches files with no further subdirectories, and the recursion naturally stops there.

**Q2. Why build a full path before recursing? What if we just called do_ls("subdir")?**

**Ans:** `opendir()` interprets a relative path relative to the process's current working directory, not relative to whatever directory we're currently listing inside the recursion. If I called `do_ls("subdir")` directly, the program would try to open "subdir" relative to wherever it was originally launched from, which is usually wrong once we're more than one level deep — it would either open the wrong folder or fail with "no such directory." Building the full path (`parent_dir/subdir`) each time keeps every `opendir()` call correctly anchored, no matter how deep the recursion goes.






