# Spec 057 (tests/arealight): the message of a failed render_pair.cmake comparison. Reports where two image files
# first differ (byte offset) and how many bytes differ. Used only for the message; the verdict is cmake -E compare_files.
import sys


def main(a, b):
    da = open(a, "rb").read()
    db = open(b, "rb").read()
    if len(da) != len(db):
        print("sizes %d and %d" % (len(da), len(db)))
        return
    diff = [i for i in range(len(da)) if da[i] != db[i]]
    if diff:
        print("first differing byte at offset %d, %d of %d bytes differ" % (diff[0], len(diff), len(da)))
    else:
        print("no differing byte")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
