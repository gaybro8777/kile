/***************************************************************************
    begin                : Mon April 17 2003
    copyright            : (C) 2006 by Thomas Braun
    email                : braun@physik.fu-berlin.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "gesymb.h"

using std::cout;

int main( int argc, char ** argv )
{
int latexret=-1;
int dvipngret=-1;
QString texcommand, texfile,type,dvipngcommand,line;
QString pkgs,pkgsarg,savepkgs="",savepkgsarg="";
bool env=false;


if (argc < 2)
	usage();


texfile=argv[2];
type=argv[1];

QFile f( texfile );
if ( !f.open( IO_ReadOnly ) )
{
	cout << "File " << texfile.toLatin1() << " is not readable\n";
	return 1;
}

texcommand="latex " + texfile;
dvipngcommand="dvipng  --picky -bg Transparent -x 518 -O -1.2in,-1.2in -T bbox -D 300 -o img%03d" + type + ".png " + texfile.left(texfile.length()-4);

cout << texcommand.toLatin1() << "\n";
cout << dvipngcommand.toLatin1() << "\n";

latexret = system(texcommand.toLatin1());
dvipngret= system(dvipngcommand.toLatin1());

if (latexret)
{
	cout << "Error compiling the latex file\n";
	return 1;
}
if(dvipngret)
{ 
	cout << "Error producing the pngs\n";
	return 1;
}

QTextStream t(&f);

QRegExp optarg("\\\\(?:math)?command\\[(.*)\\]\\{");
QRegExp arg("\\\\(?:math)?command\\{(.*)\\}");
QRegExp beginenvpkgs("^\\\\begin\\{neededpkgs\\}(?:\\[(.*)\\])?\\{(.*)\\}");
QRegExp endenvpkgs("^\\\\end\\{neededpkgs\\}");
QRegExp cmdpkgs("\\\\pkgs(?:\\[(.*)\\])?\\{(.*)\\}");

int number=1;
beginenvpkgs.setMinimal(true);
cmdpkgs.setMinimal(true);

while( (line = t.readLine()) != 0L)
{
	if(env)
	{
		pkgs=savepkgs;
		pkgsarg=savepkgsarg;
	}
	else
	{
		pkgs="";
		pkgsarg="";
	}

	if ( line.find(beginenvpkgs) != -1)
	{
		env=true;
		pkgs=beginenvpkgs.cap(2);
		pkgsarg=beginenvpkgs.cap(1);
		savepkgs=pkgs;
		savepkgsarg=pkgsarg;
	}
	else if( line.find(cmdpkgs) != -1)
	{
		pkgs=cmdpkgs.cap(2);
		pkgsarg=cmdpkgs.cap(1);
	}
	else if( line.find(endenvpkgs) != -1)
	{
		env=false;
		savepkgs="";
		savepkgsarg="";
		pkgs="";
		pkgsarg="";
	}

	cout << "line is " << line.toLatin1();
	cout << "; pkgs=" << pkgs.toLatin1() << " ,pkgsarg=" << pkgsarg.toLatin1() << " ,savepkgs=" << savepkgs.toLatin1() << " ,savepkgsarg=" << savepkgsarg.toLatin1() << "\n";

	if ( line.find(optarg) != -1)
		writeComment(optarg.cap(1),pkgs,pkgsarg,type,number++);
	else if(line.find(arg) != -1)
		writeComment(arg.cap(1),pkgs,pkgsarg,type,number++);
}

f.close();
return 0;
}

void usage()
{
	cout << "Usage:\n";
	cout << "gesymb <type> <latex-file>\n";
	exit(1);
}

void writeComment(QString cmd, QString pkgs, QString pkgsarg, QString type, int number)
{

QImage image;
QString fname;
fname.sprintf("img%03d%s.png",number,type.toLatin1());

cout << "fname is " << fname.toLatin1() << "\n";

if(image.load(fname))
{
	image.setText("Command",0,cmd);
	image.setText("Packages",0, ( pkgsarg.isEmpty() ? "" : '[' + pkgsarg + ']' ) + ( pkgs.isEmpty() ? "" : '{' + pkgs + '}') );
	
	if(!image.save(fname,"PNG"))
	{
		cout << "Image " << fname.toLatin1() << " could not be saved\n";
		exit(1);
	}
	readComment(fname);
}
else
	cout << "===writeComment=== ERROR " << fname.toLatin1() << " could not be loaded\n";

}

void readComment(QString fname)
{
QImage image;

if(image.load(fname))
{
	cout << "image " << fname.toLatin1()  << " has Command comment_" <<  image.text("Command").toLatin1() << "_\n";
	cout << "image " << fname.toLatin1()  << " has Packages comment_" <<  image.text("Packages").toLatin1() << "_\n";
	
}
else
	cout << "===readComment=== ERROR " << fname.toLatin1() << " could not be loaded\n";
}


