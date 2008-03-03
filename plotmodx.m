function plotmodx(x,y,mdx,attrib,off)
%PLOTMODX Plot with periodic x axis.
%   PLOTMODX(X,Y,MODX) plots Y vs X, first taking the mod
%   by MODX of the coordinate X.  PLOTMODX(X,Y,MODX,ATTRIB)
%   uses ATTRIB as plot attributes (see PLOT).
%   PLOTMODX(X,Y,MODX,ATTRIB,OFF) adds an offset OFF to X.
%
%   See also MOD, PLOT, PLOTMODY, PLOTMODXY, MODSPLITINTERP.

%   $Id: plotmodx.m,v 1.3 2001/08/02 14:08:08 jeanluc Exp $

if nargin < 4
    attrib = '';
end

if nargin < 5
    off = 0;
end

[xm ym] = modsplitinterp(x,y,mdx,off);

savehold = ishold;
if savehold == 0
    newplot
    hold on
end

for i = 1:length(xm)
    plot(xm{i},ym{i},attrib);
end

if savehold == 0
    hold off
end