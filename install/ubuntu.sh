pushd .

if ! test -f "/usr/local/include/sol.hpp"; then
	echo "Sol2 doesn't exist, installing from scratch!";
	pushd /tmp;
	git clone https://github.com/ThePhD/sol2.git;
	mkdir sol2/build;
	pushd sol2/build;
	cmake ../;
	echo "Silently building and installing Sol2...";
	sudo make install >/dev/null;
	popd;
	popd;
else
	echo "Sol2 already exists, skipping installation...";
fi

if ! test -f "/usr/local/include/sqlpp11/sqlpp11.h"; then
	pushd /tmp;
	echo "Sqlpp11 doesn't exist, installing from scratch!";
	git clone https://github.com/rbock/sqlpp11.git;
	mkdir sqlpp11/build;
	pushd sqlpp11/build;
	cmake ../;
	echo "Silently building and installing Sol2...";
	sudo make install >/dev/null;
	popd;
	popd;
else
	echo "Sqlpp11 already exists, skipping installation...";
fi

if ! test -f "/usr/local/include/sqlpp11/mysql/mysql.h"; then
	pushd /tmp;
	echo "Sqlpp11-connector-mysql doesn't exist, installing from scratch!";
	git clone https://github.com/rbock/sqlpp11-connector-mysql.git;
	mkdir sqlpp11-connector-mysql/build;
	pushd sqlpp11-connector-mysql/build;
	cmake ../;
	echo "Silently building and installing Sol2...";
	sudo make install >/dev/null;
	popd;
	popd;
else
	echo "Sqlpp11-connector-mysql already exists, skipping installation...";
fi

sudo apt-get -y install libreadline-dev liblua5.3-dev libboost-all-dev zlib1g-dev mariadb-server mariadb-client;

echo "/usr"
ls -al /usr
echo "/usr/local"
ls -al /usr/local/
echo "/usr/local/include"
ls -al /usr/local/include
echo "/usr/local/lib"
ls -al /usr/local/lib
echo "/usr/lib"
ls -al /usr/lib
echo "/usr/include"
ls -al /usr/include

popd;
