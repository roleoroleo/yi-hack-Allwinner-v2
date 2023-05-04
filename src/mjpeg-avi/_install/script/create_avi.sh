#!/bin/sh
# Should be called with the path to the jpgs to be converted as first argument
# resolution of the jpgs as the second argument, optional fps as third

if [ $# -le 1 ]; then
	echo "usage: $0 path_to_jpgs resolution_of_jpgs [fps]"
	exit 1
fi

path_jpgs=$1
resolution_jpgs=$2
fps_jpgs=10

if [ $# -eq 3 ]; then
	fps_jpgs=$3
fi

_dir="/tmp/sd/.jpgs"
_unique_name_count=1

while [ -d $_dir ]; do
	_dir=$_dir$_unique_name_count
	_unique_name_count=$((_unique_name_count+1))
done
mkdir $_dir

# making a directory containing the jpgs in directory 'path_jpgs' and naming them X.jpeg where X is an ascending number starting from 1.
count=1
for file in $(ls $path_jpgs | sort -n); do
	e=${file%*.jpeg}
	f=${file%*.jpg}
	if [ ! $e = $file ] || [ ! $f = $file ]; then
		cp $path_jpgs/$file $_dir/$count.jpeg
		count=$((count+1))
	fi
done

# calling the program!
if cd $path_jpgs; then
	avimake -r $resolution_jpgs -l $_dir -n $((count-1)) -s $fps_jpgs
	video_file=$(date +%Y-%m-%d_%H-%M-00)
	mv Video.avi $video_file.avi
fi

# remove processed files
cd /tmp
rm -r $path_jpgs/*.jpg
rm -r $path_jpgs/*.jpeg

# cleanup
if cd $_dir; then
	rm *
	cd ..
	rmdir $_dir
fi
