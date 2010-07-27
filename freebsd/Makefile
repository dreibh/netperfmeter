# New ports collection makefile for: netperfmeter
# Date created:                      27 July 2010
# Whom:                              Thomas Dreibholz <dreibh@iem.uni-due.de>
#
# $FreeBSD$
#

PORTNAME=	netperfmeter
PORTVERSION=	1.0.4
CATEGORIES=	net
MASTER_SITES=	http://www.iem.uni-due.de/~dreibh/netperfmeter/download/
DISTNAME=	${PORTNAME}-${PORTVERSION}
EXTRACT_SUFX=	.tar.gz

PATCH_SITES=
PATCHFILES=

MAINTAINER=	dreibh@iem.uni-due.de
COMMENT=	Network Performance Meter

RUN_DEPENDS=	bzip2:${PORTSDIR}/archivers/bzip2
LIB_DEPENDS=	bz2.1:${PORTSDIR}/archivers/bzip2

#MAN8=          netperfmeter.8
#MANCOMPRESSED= yes

IS_INTERACTIVE=	no
GNU_CONFIGURE=	yes
USE_GMAKE=	yes


pre-fetch:

post-patch:

pre-install:


.include <bsd.port.mk>
