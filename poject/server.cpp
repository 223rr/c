// 用一个极简本地 HTTP 服务把任务管理器暴露给网页。
// 只监听 127.0.0.1，所有接口都是 GET + 查询参数，页面用 fetch 调用。
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <shellapi.h>
#endif

#include "task.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

const char kIndexHtml[] = R"HTML(<<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>任务手账</title>
<style>
  :root {
    --desk: #e8dcc4;
    --paper: #fffdf4;
    --ink: #2b2a26;
    --ink-soft: #6d6656;
    --rule: #e7ddc6;
    --red: #9e4232;
    --blue: #2f5d8a;
    --tape: rgba(238, 219, 148, .72);
    --p3-bg: #f1d9d4; --p3-fg: #8f3b30;
    --p2-bg: #efe3c6; --p2-fg: #806025;
    --p1-bg: #dbe6d7; --p1-fg: #3f6140;
    color-scheme: light;
  }
  * { box-sizing: border-box; }

  body {
    margin: 0;
    min-height: 100vh;
    padding: 44px 18px 90px;
    color: var(--ink);
    font: 16px/1.85 "KaiTi", "STKaiti", "楷体", "Kaiti SC", Georgia, serif;
    background-color: var(--desk);
    background-image:
      radial-gradient(circle at 18% 12%, rgba(255,255,255,.5), transparent 45%),
      radial-gradient(circle at 82% 6%, rgba(210,190,150,.55), transparent 50%),
      repeating-linear-gradient(92deg, rgba(160,140,100,.05) 0 1px, transparent 1px 7px),
      repeating-linear-gradient(2deg, rgba(160,140,100,.04) 0 1px, transparent 1px 9px);
  }
  body::after {
    content: "";
    position: fixed; inset: 0; z-index: 0; pointer-events: none;
    opacity: .5;
    background-image: url("data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' width='140' height='140'><filter id='n'><feTurbulence type='fractalNoise' baseFrequency='0.85' numOctaves='3' stitchTiles='stitch'/></filter><rect width='140' height='140' filter='url(%23n)' opacity='0.14'/></svg>");
  }

  .wrap { max-width: 780px; margin: 0 auto; position: relative; z-index: 1; }

  .sheet {
    position: relative;
    background: var(--paper);
    border: 1px solid #e3d8bd;
    border-radius: 3px;
    box-shadow:
      0 1px 0 #fff inset,
      0 18px 34px -22px rgba(90, 74, 40, .5),
      0 2px 6px rgba(90, 74, 40, .1);
    padding: 26px 30px 24px 46px;
  }
  .sheet::before {
    content: "";
    position: absolute; left: 16px; top: 26px; bottom: 26px;
    width: 10px;
    background-image: radial-gradient(circle, #d8cbaf 0 3.2px, transparent 3.6px);
    background-size: 10px 26px;
    background-repeat: repeat-y;
    opacity: .9;
  }

  .clip {
    position: absolute; top: -17px; right: 30px;
    width: 34px; height: 64px;
    color: #9c9074; transform: rotate(9deg); opacity: .85;
    pointer-events: none;
  }

  header { position: relative; margin-bottom: 20px; padding: 0 104px 12px 0; }
  header::after {
    content: "";
    position: absolute; left: 0; right: 0; bottom: 0;
    height: 2px;
    background: linear-gradient(90deg, var(--ink) 0 58%, transparent 58%);
    opacity: .5;
  }
  .eyebrow {
    display: inline-block;
    font-size: 12.5px; font-weight: 700;
    letter-spacing: .2em; text-transform: uppercase;
    color: var(--red);
    border: 1.5px solid var(--red);
    padding: 2px 9px 1px;
    transform: rotate(-3deg);
    border-radius: 2px;
    opacity: .9;
  }
  h1 {
    margin: 12px 0 2px;
    font-size: 34px; font-weight: 700;
    letter-spacing: .06em;
    transform: rotate(-.8deg);
  }
  .sub { color: var(--ink-soft); font-size: 13px; }
  .sub .cloud { margin-right: 4px; opacity: .7; }

  .note {
    position: absolute; top: 0; right: 0;
    background: #f7ea9e;
    padding: 7px 14px 9px;
    transform: rotate(3.2deg);
    box-shadow: 0 6px 12px -8px rgba(90,74,40,.8);
    text-align: center; color: #6f6028;
  }
  .note span { display: block; font-size: 11px; letter-spacing: .24em; }
  .note strong { display: block; font-size: 21px; line-height: 1.25; }

  .tape {
    position: absolute; top: -13px; left: 42%;
    width: 150px; height: 27px;
    background: var(--tape);
    border-left: 1px dashed rgba(120,100,60,.28);
    border-right: 1px dashed rgba(120,100,60,.28);
    transform: rotate(-1.6deg);
    box-shadow: 0 2px 4px rgba(120,100,60,.12);
  }

  /* ---------- 录入 ---------- */
  .composer { margin-bottom: 8px; }
  .quickrow { display: flex; gap: 12px; align-items: flex-end; }
  .quickrow .field { flex: 1 1 auto; min-width: 0; }
  .field-label {
    display: block;
    font-size: 11.5px; letter-spacing: .18em; text-transform: uppercase;
    color: var(--ink-soft);
    margin: 0 0 1px 1px;
    font-family: "Segoe UI", system-ui, sans-serif;
  }

  input[type=text], input[type=date], input[type=search] {
    width: 100%;
    font: inherit; color: var(--ink);
    background: transparent;
    border: 0;
    border-bottom: 1px solid rgba(205, 193, 165, .55);
    border-radius: 0;
    padding: 4px 2px 5px;
    outline: none;
    transition: border-color .22s, background .22s;
  }
  input[type=text]:focus, input[type=date]:focus, input[type=search]:focus {
    border-bottom: 1.5px solid var(--red);
    background: rgba(255, 250, 235, .75);
  }
  input::placeholder { color: #bbb197; }
  select {
    font: inherit; color: var(--ink); background: transparent;
    border: 0; border-bottom: 1px solid rgba(205, 193, 165, .55);
    border-radius: 0; padding: 4px 2px 5px; width: 100%; outline: none;
    transition: border-color .22s;
  }
  select:focus { border-bottom-color: var(--red); }
  input[type=date] { cursor: pointer; }
  input[type=date]::-webkit-datetime-edit { color: #bcb29a; font-size: 14.5px; }
  input[type=date].filled::-webkit-datetime-edit { color: var(--ink); font-size: 16px; }

  .stamp-btn {
    font: inherit; font-size: 15px; letter-spacing: .14em;
    color: #fff6e6;
    background: linear-gradient(180deg, #ac4a39, #8f3d2d);
    border: 0;
    border-radius: 6px;
    padding: 10px 24px 11px;
    cursor: pointer;
    box-shadow:
      0 10px 18px -13px rgba(110, 42, 30, .95),
      0 1px 0 rgba(255, 255, 255, .18) inset;
    transform: rotate(-1.2deg);
    transition: transform .15s, filter .15s, box-shadow .2s;
    flex: 0 0 auto;
  }
  .stamp-btn:hover { filter: brightness(1.07); }
  .stamp-btn:active { transform: rotate(-1.2deg) translateY(1px) scale(.985); }
  .stamp-btn.busy { animation: stamp .34s ease; }
  @keyframes stamp {
    0% { transform: rotate(-1.2deg) scale(1); }
    45% { transform: rotate(-2.4deg) scale(.94); }
    100% { transform: rotate(-1.2deg) scale(1); }
  }

  .hint-err {
    color: var(--red); font-size: 12.5px; margin-top: 4px;
    min-height: 0; opacity: 0; transition: opacity .18s;
  }
  .hint-err.show { opacity: 1; }
  .shake { animation: shake .32s ease; }
  @keyframes shake {
    0%, 100% { transform: translateX(0); }
    25% { transform: translateX(-4px); }
    75% { transform: translateX(4px); }
  }
  .bad { border-bottom-color: var(--red) !important; }

  .parsed {
    font-size: 12.5px; color: var(--ink-soft);
    margin-top: 6px; min-height: 19px;
    opacity: 0; transition: opacity .18s;
  }
  .parsed.show { opacity: 1; }
  .parsed b { color: var(--red); font-weight: 700; }

  /* 详细选项：聚焦或已有内容时展开 */
  .details {
    overflow: hidden;
    max-height: 0;
    opacity: 0;
    transition: max-height .32s ease, opacity .28s ease, margin-top .32s ease;
  }
  .details.open { max-height: 220px; opacity: 1; margin-top: 14px; }
  .detail-grid {
    display: grid;
    grid-template-columns: minmax(0, 1fr) 160px 160px;
    gap: 12px 16px;
    align-items: center;
    padding-top: 12px;
    border-top: 1px dashed #ded0b0;
  }
  .seg { display: inline-flex; gap: 8px; align-items: center; }
  .seg button {
    font: inherit; font-size: 14.5px; letter-spacing: .08em;
    color: var(--ink-soft);
    background: transparent;
    border: 1px solid rgba(207, 193, 161, .9);
    border-radius: 999px;
    padding: 4px 17px 5px;
    cursor: pointer;
    transition: background .16s, color .16s, border-color .16s;
  }
  .seg button:hover { border-color: var(--ink-soft); }
  .seg button[data-v="3"][aria-pressed="true"] { background: var(--p3-bg); color: var(--p3-fg); border-color: var(--p3-bg); }
  .seg button[data-v="2"][aria-pressed="true"] { background: var(--p2-bg); color: var(--p2-fg); border-color: var(--p2-bg); }
  .seg button[data-v="1"][aria-pressed="true"] { background: var(--p1-bg); color: var(--p1-fg); border-color: var(--p1-bg); }
  .seg button[aria-pressed="true"] { font-weight: 700; }

  .more-toggle {
    font: inherit; font-size: 12.5px; color: var(--ink-soft);
    background: transparent; border: 0; cursor: pointer;
    padding: 2px 0; margin-top: 4px;
    border-bottom: 1px dashed rgba(160,145,115,.6);
  }
  .more-toggle:hover { color: var(--red); }

  /* ---------- 查看栏 ---------- */
  .viewbar {
    display: grid;
    grid-template-columns: minmax(0, 1fr) auto;
    gap: 12px 20px;
    align-items: center;
    margin: 18px 0 4px;
    padding-top: 14px;
    border-top: 1px dashed #ded0b0;
  }
  #search { border-bottom-color: rgba(205, 193, 165, .38); }
  #search:focus { border-bottom-color: var(--red); }
  .sortseg { display: inline-flex; gap: 2px; align-items: center; }
  .sortseg button {
    font: inherit; font-size: 15px;
    color: var(--ink-soft);
    background: transparent; border: 0;
    padding: 6px 10px 7px;
    cursor: pointer;
    border-bottom: 2px solid transparent;
    border-radius: 3px 3px 0 0;
    transition: color .15s, border-color .15s, background .15s;
    white-space: nowrap;
  }
  .sortseg button:hover { color: var(--ink); background: rgba(255, 246, 214, .6); }
  .sortseg button[aria-pressed="true"] {
    color: var(--red); border-bottom-color: var(--red); font-weight: 700;
  }
  .listfoot { display: flex; justify-content: space-between; align-items: center; gap: 12px; padding-top: 8px; }
  .count { font-size: 12.5px; color: #a2977c; letter-spacing: .06em; }
  .foot-actions { display: flex; gap: 14px; }
  .foot-actions button, .foot-actions a {
    font: inherit; font-size: 13px; color: var(--ink-soft);
    background: transparent; border: 0;
    border-bottom: 1px dashed rgba(160, 145, 115, .55);
    padding: 0; cursor: pointer; text-decoration: none;
  }
  .foot-actions button:hover, .foot-actions a:hover { color: var(--red); border-color: var(--red); }
  .foot-actions button:disabled { opacity: .45; cursor: default; border-color: transparent; }

  mark { background: #f6e6a8; color: inherit; padding: 0 1px; border-radius: 2px; }

  .toast {
    position: fixed; left: 50%; bottom: 26px; z-index: 40;
    transform: translate(-50%, 16px);
    display: flex; align-items: center; gap: 14px;
    background: #2b2a26; color: #f7f1e2;
    padding: 10px 16px; border-radius: 6px; font-size: 14px;
    box-shadow: 0 12px 24px -14px rgba(0, 0, 0, .8);
    opacity: 0; pointer-events: none;
    transition: opacity .22s, transform .22s;
  }
  .toast.show { opacity: 1; transform: translate(-50%, 0); pointer-events: auto; }
  .toast button {
    font: inherit; font-size: 13.5px; color: #f3d38a;
    background: transparent; border: 0;
    border-bottom: 1px solid #f3d38a;
    padding: 0; cursor: pointer;
  }

  /* 任务详情抽屉 */
  .drawer-backdrop {
    position: fixed; inset: 0; z-index: 24;
    background: rgba(60, 48, 30, .28);
    opacity: 0; pointer-events: none; transition: opacity .25s;
  }
  .drawer-backdrop.show { opacity: 1; pointer-events: auto; }
  .drawer {
    position: fixed; top: 0; right: 0; bottom: 0; z-index: 25;
    width: min(430px, 94vw);
    display: flex; flex-direction: column;
    background: var(--paper);
    border-left: 1px solid #e3d8bd;
    box-shadow: -22px 0 44px -28px rgba(60, 48, 30, .8);
    transform: translateX(100%);
    transition: transform .28s cubic-bezier(.2, .8, .2, 1);
    padding: 20px 22px 16px;
  }
  .drawer.show { transform: none; }
  .drawer-head { display: flex; align-items: flex-start; gap: 10px; padding-bottom: 10px; border-bottom: 1px dashed #ded0b0; }
  .drawer-head h2 { margin: 0; font-size: 19px; line-height: 1.4; flex: 1; word-break: break-word; }
  .drawer-head .close {
    border: 0; background: transparent; cursor: pointer;
    font-size: 20px; line-height: 1; color: var(--ink-soft); padding: 0 2px;
  }
  .drawer-head .close:hover { color: var(--red); }  .drawer-body { flex: 1 1 auto; overflow: auto; padding-top: 12px; }
  #notes {
    width: 100%; min-height: 170px;
    font: inherit; font-size: 15px; line-height: 28px; color: var(--ink);
    background: repeating-linear-gradient(transparent 0 27px, rgba(200, 190, 165, .45) 27px 28px);
    border: 0; outline: none; resize: vertical; padding: 0 2px;
  }
  .links { margin-top: 8px; display: flex; flex-direction: column; gap: 2px; }
  .links a { color: var(--blue); font-size: 13.5px; word-break: break-all; }
  .files { list-style: none; margin: 4px 0 0; padding: 0; }
  .files li { display: flex; align-items: center; gap: 8px; padding: 3px 0; border-bottom: 0; font-size: 14px; }
  .files a { color: var(--blue); word-break: break-all; flex: 1; }
  .files button { border: 0; background: none; color: #c0b295; cursor: pointer; font-size: 15px; }
  .files button:hover { color: var(--red); }
  .upload {
    display: inline-block; margin-top: 8px;
    font-size: 13.5px; color: var(--red); cursor: pointer;
    border-bottom: 1px dashed rgba(158, 66, 50, .6);
  }
  .drawer-foot {
    display: flex; align-items: center; justify-content: space-between; gap: 10px;
    padding-top: 12px; border-top: 1px dashed #ded0b0;
  }
  .drawer-foot .muted { font-size: 12.5px; color: var(--ink-soft); }
  .stamp-btn.small { font-size: 13.5px; padding: 6px 16px 7px; letter-spacing: .1em; }

  /* 回收站 */
  .trash { list-style: none; margin: 0; padding: 0; }
  .trash li {
    display: flex; align-items: baseline; gap: 10px;
    padding: 9px 0; border-bottom: 1px dashed var(--rule); font-size: 15px;
  }
  .trash .t-title { flex: 1; min-width: 0; word-break: break-word; }
  .trash .t-meta { font-size: 12.5px; color: #9a9078; white-space: nowrap; }
  .trash button {
    font: inherit; font-size: 13px; color: var(--red);
    background: none; border: 0; padding: 0; cursor: pointer;
    border-bottom: 1px dashed rgba(158, 66, 50, .6);
  }
  .trash .empty-line { color: #a2977c; font-size: 14px; border: 0; }

  /* 复盘 / 打卡 */
  .modal.wide { max-width: 620px; }
  .modal textarea {
    width: 100%; min-height: 110px; resize: vertical;
    font: inherit; font-size: 15px; line-height: 27px; color: var(--ink);
    background: repeating-linear-gradient(transparent 0 26px, rgba(200, 190, 165, .45) 26px 27px);
    border: 0; outline: none; padding: 0 2px; margin: 6px 0 14px;
  }
  .heat {
    display: grid;
    grid-template-rows: repeat(7, 13px);
    grid-auto-flow: column;
    grid-auto-columns: 13px;
    gap: 3px;
    justify-content: start;
    overflow-x: auto;
    padding: 4px 0 10px;
  }
  .heat i { width: 13px; height: 13px; border-radius: 2px; background: #efe7cf; display: block; }
  .heat i[data-l="1"] { background: #dfe5c4; }
  .heat i[data-l="2"] { background: #c3d6a8; }
  .heat i[data-l="3"] { background: #9dbe86; }
  .heat i[data-l="4"] { background: #74986a; }
  .heat i.blank { background: transparent; }
  .heat-legend { font-size: 12px; color: var(--ink-soft); display: flex; gap: 6px; align-items: center; }
  .heat-legend i { width: 11px; height: 11px; border-radius: 2px; display: inline-block; background: #efe7cf; }
  .heat-legend i[data-l="2"] { background: #c3d6a8; }
  .heat-legend i[data-l="4"] { background: #74986a; }

  /* 贴纸 / 专注计时 */
  .stickers { display: flex; gap: 6px; flex-wrap: wrap; margin-top: 4px; }
  .stickers button {
    font-size: 18px; line-height: 1; padding: 4px 6px;
    background: transparent; border: 1px solid transparent; border-radius: 6px;
    cursor: pointer; filter: grayscale(.25); transition: all .15s;
  }
  .stickers button:hover { background: rgba(255, 246, 214, .8); }
  .stickers button[aria-pressed="true"] { border-color: var(--red); background: rgba(255, 246, 214, .9); filter: none; }
  .drawer-tools { display: flex; align-items: center; gap: 12px; margin-top: 16px; }
  .drawer-tools .muted { font-size: 12.5px; color: var(--ink-soft); }
  .sticker-mark { margin-right: 6px; }

  .pomo {
    position: fixed; inset: 0; z-index: 50;
    display: none; align-items: center; justify-content: center;
    background: rgba(40, 32, 20, .55);
    backdrop-filter: blur(2px);
  }
  .pomo.show { display: flex; }
  .pomo-card {
    background: var(--paper);
    border: 1px solid #e3d8bd;
    box-shadow: 0 30px 60px -30px rgba(0, 0, 0, .8);
    padding: 34px 46px 28px;
    text-align: center;
    transform: translate(var(--tx, 0px), var(--ty, 0px)) rotate(-.6deg);
    min-width: 300px;
    cursor: grab;
  }
  .pomo-label { font-size: 14px; letter-spacing: .2em; color: var(--ink-soft); }
  .pomo-title { font-size: 17px; margin: 6px 0 14px; color: var(--ink); }
  .pomo-time {
    font-family: "Consolas", "SF Mono", monospace;
    font-size: 56px; line-height: 1.1; letter-spacing: .04em;
    color: var(--red);
    font-variant-numeric: tabular-nums;
  }
  .pomo-actions { display: flex; gap: 10px; justify-content: center; margin-top: 22px; }
  .pomo-actions button {
    font: inherit; font-size: 13.5px; padding: 7px 16px;
    border-radius: 4px; cursor: pointer; border: 1px solid var(--red);
    background: var(--red); color: #fff6e6;
  }
  .pomo-actions button.ghost { background: transparent; color: var(--ink-soft); border-color: #cdbf9f; }

  .moods { display: flex; gap: 8px; margin: 4px 0 12px; }
  .moods button {
    font: inherit; font-size: 13.5px; padding: 4px 12px 5px;
    border: 1px solid rgba(207, 193, 161, .9); border-radius: 999px;
    background: transparent; cursor: pointer; color: var(--ink-soft);
  }
  .moods button[aria-pressed="true"] { background: var(--p2-bg); border-color: var(--p2-bg); color: var(--p2-fg); font-weight: 700; }

  /* 标签 */
  .tagchip {
    font: inherit; font-size: 12.5px; color: var(--red);
    background: rgba(255, 246, 214, .9);
    border: 1px solid rgba(158, 66, 50, .45);
    border-radius: 999px; padding: 1px 10px; cursor: pointer;
  }
  .filterline { font-size: 12.5px; color: var(--ink-soft); padding-top: 10px; display: flex; align-items: center; gap: 6px; }
  .taglist { list-style: none; margin: 0; padding: 0; }
  .taglist li { display: flex; align-items: center; gap: 10px; padding: 9px 0; border-bottom: 1px dashed var(--rule); }
  .taglist button {
    font: inherit; font-size: 14.5px; color: var(--ink);
    background: transparent; border: 0; cursor: pointer; padding: 0;
    border-bottom: 1px dashed rgba(160, 145, 115, .55);
  }
  .taglist button:hover { color: var(--red); }
  .taglist .tagcount { color: #a2977c; font-size: 12.5px; margin-left: auto; }
  .taglist .tagchip { color: var(--red); }
  .tag { font-size: 12px; color: #8a7c62; }

  /* ---------- 列表 ---------- */
  ul { list-style: none; margin: 0; padding: 0; }
  li {
    display: flex; align-items: stretch; gap: 10px;
    padding: 12px 2px 12px 0;
    border-bottom: 1px solid var(--rule);
    position: relative;
  }
  li:hover::after {
    content: "";
    position: absolute; inset: 0 -6px 0 -10px;
    background: rgba(255, 246, 214, .55);
    z-index: -1; border-radius: 3px;
  }
  li.fresh { animation: drop .45s cubic-bezier(.2,.9,.3,1.2); }
  @keyframes drop {
    0% { opacity: 0; transform: translateY(-12px) rotate(-.6deg); }
    70% { opacity: 1; transform: translateY(2px); }
    100% { transform: none; }
  }

  .prio-bar {
    flex: 0 0 3px; width: 3px; border-radius: 2px;
    background: #cfc4a9; margin: 2px 0;
  }
  .prio-bar.p3 { background: #b5534a; }
  .prio-bar.p2 { background: #b39143; }
  .prio-bar.p1 { background: #6f8f68; }

  .check {
    flex: 0 0 auto;
    width: 22px; height: 22px; margin-top: 4px;
    border: 1.6px solid #b9ab8c;
    border-radius: 50%;
    background: transparent;
    cursor: pointer;
    display: grid; place-items: center;
    font-size: 14px; line-height: 1;
    color: transparent;
    transition: all .18s;
  }
  .check:hover { border-color: var(--red); }
  .done .check { color: var(--red); border-color: var(--red); transform: rotate(-8deg); }
  /* 打勾后整行淡下去，鼠标移入恢复，方便再操作 */
  li.done { opacity: .5; transition: opacity .35s ease; }
  li.done:hover, li.done:focus-within { opacity: 1; }

  .body { flex: 1 1 auto; min-width: 0; }
  .row-top { display: flex; gap: 14px; align-items: baseline; }
  .title {
    position: relative;
    font-size: 18px; font-weight: 600; letter-spacing: .01em;
    word-break: break-word;
    transition: color .3s;
  }
  .title::after {
    content: "";
    position: absolute; left: 0; right: 0; top: 55%;
    height: 1.6px;
    background: rgba(158, 66, 50, .5);
    transform: scaleX(0); transform-origin: left;
    transition: transform .38s cubic-bezier(.4,0,.2,1);
    pointer-events: none;
  }
  .done .title { color: #b3a894; }
  .done .title::after { transform: scaleX(1); }
  .done .meta { opacity: .5; }

  .due {
    margin-left: auto; flex: 0 0 auto;
    font-size: 12.5px; color: #a89d84; white-space: nowrap;
  }
  .due.late { color: var(--red); }

  .meta {
    display: flex; gap: 10px; align-items: baseline; flex-wrap: wrap;
    margin-top: 3px; font-size: 12.5px; color: #9a9078;
  }
  /* 子任务 */
  .subbtn {
    font: inherit; font-size: 12px; color: var(--ink-soft);
    background: transparent; border: 1px solid rgba(200, 190, 165, .85);
    border-radius: 999px; padding: 0 9px; cursor: pointer;
    transition: color .15s, border-color .15s;
  }
  .subbtn:hover { color: var(--red); border-color: var(--red); }
  .subs { margin: 6px 0 2px; padding: 4px 0 4px 8px; border-left: 2px solid rgba(200, 190, 165, .5); }
  .sub { display: flex; align-items: center; gap: 8px; padding: 2px 0; font-size: 14px; }
  .subitem { display: flex; align-items: center; gap: 8px; flex: 1; cursor: pointer; min-width: 0; }
  .subitem span { word-break: break-word; }
  .subitem input { accent-color: #9e4232; }
  .subitem.done span { text-decoration: line-through; color: #b9ad96; }
  .subdel {
    background: none; border: 0; color: #c0b295; cursor: pointer;
    font-size: 15px; line-height: 1; padding: 0 4px;
  }
  .subdel:hover { color: var(--red); }
  .subadd { margin-top: 4px; }
  .subadd input {
    width: 100%; font: inherit; font-size: 14px; color: var(--ink);
    background: transparent; border: 0;
    border-bottom: 1px dashed rgba(200, 190, 165, .7);
    padding: 3px 2px; outline: none;
  }
  .subadd input:focus { border-bottom-color: var(--red); }
  .actions { flex: 0 0 auto; align-self: center; opacity: 0; transition: opacity .18s; }
  li:hover .actions, li:focus-within .actions { opacity: 1; }
  .actions button {
    font: inherit; font-size: 13px;
    color: var(--ink-soft);
    background: transparent; border: 0;
    border-bottom: 1.5px solid #c9bb9b;
    padding: 0 2px; cursor: pointer;
  }
  .actions button:hover { color: var(--red); border-color: var(--red); }

  .empty { padding: 26px 0 28px; text-align: center; color: #a2977c; }
  .empty svg { display: block; margin: 0 auto 2px; color: #b8ab8b; }
  .empty .art { font-size: 14px; letter-spacing: .08em; }
  .empty button {
    font: inherit; font-size: 13.5px; color: var(--red);
    background: transparent; border: 0; cursor: pointer;
    border-bottom: 1px dashed rgba(158,66,50,.6);
    margin-top: 6px;
  }

  .status { min-height: 18px; font-size: 13px; color: var(--ink-soft); padding-top: 8px; }
  .status.err { color: var(--red); }
  .status.ok { color: var(--p1-fg); }

  /* 提醒弹窗 */
  .modal-backdrop {
    position: fixed; inset: 0; z-index: 30;
    background: rgba(60, 48, 30, .32);
    display: none; align-items: center; justify-content: center;
    padding: 20px;
  }
  .modal-backdrop.show { display: flex; }
  .modal {
    position: relative;
    width: 100%; max-width: 430px;
    background: var(--paper);
    border: 1px solid #e3d8bd;
    box-shadow: 0 22px 46px -20px rgba(60, 48, 30, .75);
    padding: 26px 28px 20px;
    /* 拖动时改 --tx/--ty 即可，旋转不变 */
    transform: translate(var(--tx, 0px), var(--ty, 0px)) rotate(-.5deg);
    animation: pop .26s ease both;
  }
  .modal h2 { cursor: grab; user-select: none; }
  .dragging { cursor: grabbing; user-select: none; }
  .dragging h2 { cursor: grabbing; }
  @keyframes pop {
    from {
      opacity: 0;
      transform: translate(var(--tx, 0px), calc(var(--ty, 0px) - 10px)) rotate(-.5deg) scale(.97);
    }
    to { opacity: 1; transform: translate(var(--tx, 0px), var(--ty, 0px)) rotate(-.5deg); }
  }
  .modal h2 { margin: 0 0 2px; font-size: 21px; letter-spacing: .06em; color: var(--red); }
  .modal .hint { margin: 0 0 12px; font-size: 13px; color: var(--ink-soft); }
  .modal ul { margin: 0 0 16px; max-height: 40vh; overflow: auto; }
  .modal li {
    display: flex; gap: 8px; align-items: baseline;
    padding: 7px 0; border-bottom: 1px dashed var(--rule); font-size: 15px;
  }
  .modal li:last-child { border-bottom: 0; }
  .modal li .when { color: var(--red); font-size: 13px; white-space: nowrap; }
  .modal .actions-row { text-align: right; }
  .modal button {
    font: inherit; font-size: 14px; letter-spacing: .12em;
    color: #fff8ea; background: var(--red);
    border: 0; border-radius: 3px;
    padding: 7px 22px 8px; cursor: pointer;
  }
  .modal button.cancel {
    background: transparent; color: var(--ink-soft);
    border: 1px solid #cdbf9f; margin-right: 8px;
  }
  .modal button.cancel:hover { color: var(--red); border-color: var(--red); filter: none; }

  footer {
    text-align: center; color: #9d9179; font-size: 12px;
    letter-spacing: .18em; margin-top: 22px;
  }

  .doodle {
    position: fixed; right: 14px; bottom: 6px;
    width: 132px; height: 132px;
    color: #a4977a; opacity: .45;
    pointer-events: none; z-index: 0;
  }
  .doodle-left {
    position: fixed; left: 10px; bottom: 14px;
    width: 96px; height: 96px;
    color: #a4977a; opacity: .4;
    transform: rotate(-8deg);
    pointer-events: none; z-index: 0;
  }

  @media (max-width: 720px) { .doodle, .doodle-left { display: none; } }
  @media (max-width: 620px) {
    body { padding: 26px 10px 46px; }
    .sheet { padding: 22px 18px 20px 36px; }
    header { padding-right: 0; }
    h1 { font-size: 27px; }
    .note { position: static; display: inline-block; margin-top: 10px; transform: rotate(-2deg); }
    .tape { width: 110px; left: 30%; }
    .detail-grid, .viewbar { grid-template-columns: minmax(0, 1fr); }
    .details.open { max-height: 320px; }
    .actions { opacity: 1; }
  }
</style>
</head>
<body>

<svg class="doodle" viewBox="0 0 120 120" fill="none" stroke="currentColor" stroke-width="1.8"
     stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">
  <path d="M28 48h50v28a17 17 0 0 1-17 17H45a17 17 0 0 1-17-17z"/>
  <path d="M78 56h9a8 8 0 0 1 0 16h-9"/>
  <path d="M41 36c0-5 4-5 4-10"/><path d="M53 36c0-5 4-5 4-10"/><path d="M65 36c0-5 4-5 4-10"/>
  <path d="M22 102h62"/>
</svg>
<svg class="doodle-left" viewBox="0 0 100 100" fill="none" stroke="currentColor" stroke-width="1.8"
     stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">
  <path d="M32 62h36l-4 26H36z"/><path d="M50 62V34"/>
  <path d="M50 42c-9 0-14-5-14-13 9 0 14 5 14 13z"/>
  <path d="M50 50c9 0 14-5 14-13-9 0-14 5-14 13z"/>
</svg>

<div class="wrap">
  <section class="sheet">
    <span class="tape" aria-hidden="true"></span>
    <svg class="clip" viewBox="0 0 34 64" fill="none" stroke="currentColor" stroke-width="2.4"
         stroke-linecap="round" aria-hidden="true">
      <path d="M22 15v31a7.5 7.5 0 0 1-15 0V18a5.5 5.5 0 0 1 11 0v26"/>
    </svg>

    <header>
      <span class="eyebrow">to-do diary</span>
      <h1>任务手账</h1>
      <div class="sub" title="数据保存在服务端 tasks.txt，由本机 127.0.0.1 的 C++ 服务读写">
        <span class="cloud">☁</span>已同步到本机手账
      </div>
      <div class="note"><span>今日</span><strong id="noteDate">--/--</strong></div>
    </header>

    <form id="addForm" class="composer">
      <div class="quickrow">
        <label class="field">
          <input type="text" id="title" placeholder="写点什么…试试「明天开会 ！高」"
                 maxlength="200" autocomplete="off" aria-label="任务内容">
        </label>
        <button class="stamp-btn" type="submit" id="addBtn">记下</button>
      </div>
      <div class="hint-err" id="hint">请写下任务内容</div>
      <div class="parsed" id="parsed"></div>

      <div class="details" id="details">
        <div class="detail-grid">
          <div class="field">
            <span class="field-label">轻重缓急</span>
            <div class="seg" id="priority" role="group" aria-label="优先级">
              <button type="button" data-v="3" aria-pressed="false">高</button>
              <button type="button" data-v="2" aria-pressed="true">中</button>
              <button type="button" data-v="1" aria-pressed="false">低</button>
            </div>
          </div>
          <label class="field">
            <span class="field-label">截止日</span>
            <input type="date" id="due" aria-label="截止日期">
          </label>
          <label class="field" id="targetField" hidden>
            <span class="field-label">目标完成日</span>
            <input type="date" id="target" aria-label="目标完成日">
          </label>
          <label class="field">
            <span class="field-label">重复</span>
            <select id="repeat" aria-label="重复规则">
              <option value="">不重复</option>
              <option value="daily">每天</option>
              <option value="weekly">每周</option>
              <option value="monthly">每月</option>
            </select>
          </label>
          <label class="field">
            <span class="field-label">标签</span>
            <input type="text" id="tags" placeholder="工作 生活" maxlength="60" aria-label="标签">
          </label>
        </div>
        <button type="button" class="more-toggle" id="moreToggle">＋ 目标完成日</button>
      </div>
    </form>

    <div class="viewbar">
      <label class="field">
        <span class="field-label">搜索</span>
        <input type="search" id="search" placeholder="搜索任务…" aria-label="搜索现有任务">
      </label>
      <div class="field">
        <span class="field-label">排列</span>
        <div class="sortseg" id="sort" role="group" aria-label="排序方式">
          <button type="button" data-sort="id" aria-pressed="true">编号</button>
          <button type="button" data-sort="priority" aria-pressed="false">轻重</button>
          <button type="button" data-sort="date" aria-pressed="false">日期</button>
        </div>
      </div>
    </div>

    <ul id="tasks"></ul>
    <div class="filterline" id="filterLine" hidden>
      正在看 <button type="button" class="tagchip" id="tagFilter"></button>
    </div>
    <div class="listfoot">
      <div class="foot-actions">
        <button type="button" id="clearDone">清空已完成</button>
        <button type="button" id="openTrash">回收站</button>
        <button type="button" id="openTags">标签</button>
        <button type="button" id="openReview">今日回顾</button>
        <button type="button" id="openStats">打卡</button>
        <a href="/api/export.md">导出 Markdown</a>
      </div>
      <span class="count" id="count"></span>
    </div>
    <div class="status" id="status"></div>

    <footer>— 本地手账 · 仅 127.0.0.1 —</footer>
  </section>
</div>

<div class="toast" id="toast">
  <span id="toastText"></span>
  <button type="button" id="toastUndo">撤销</button>
</div>

<div class="drawer-backdrop" id="drawerBack"></div>
<aside class="drawer" id="drawer" aria-hidden="true" aria-label="任务详情">
  <div class="drawer-head">
    <h2 id="drawerTitle">任务详情</h2>
    <button class="close" id="drawerClose" type="button" aria-label="关闭">×</button>
  </div>
  <div class="drawer-body">
    <span class="field-label">备注</span>
    <textarea id="notes" placeholder="写点细节、贴个链接…"></textarea>
    <div class="links" id="links"></div>

    <span class="field-label" style="display:block;margin-top:14px">贴纸</span>
    <div class="stickers" id="stickers"></div>

    <span class="field-label" style="display:block;margin-top:14px">附件</span>
    <ul class="files" id="files"></ul>
    <label class="upload">＋ 添加附件<input type="file" id="fileInput" multiple hidden></label>

    <div class="drawer-tools">
      <button class="stamp-btn small" id="startFocus" type="button">开始专注</button>
      <span class="muted" id="spentText"></span>
    </div>
  </div>
  <div class="drawer-foot">
    <span class="muted" id="drawerStatus"></span>
    <button class="stamp-btn small" id="saveNotes" type="button">保存备注</button>
  </div>
</aside>

<div class="drawer-backdrop" id="tagBack"></div>
<aside class="drawer" id="tagDrawer" aria-hidden="true" aria-label="标签">
  <div class="drawer-head">
    <h2>标签</h2>
    <button class="close" id="tagClose" type="button" aria-label="关闭">×</button>
  </div>
  <div class="drawer-body">
    <ul class="taglist" id="tagList"></ul>
  </div>
</aside>

<div class="drawer-backdrop" id="trashBack"></div>
<aside class="drawer" id="trashDrawer" aria-hidden="true" aria-label="回收站">
  <div class="drawer-head">
    <h2>回收站</h2>
    <button class="close" id="trashClose" type="button" aria-label="关闭">×</button>
  </div>
  <div class="drawer-body">
    <ul class="trash" id="trashList"></ul>
  </div>
</aside>

<div class="pomo" id="pomo" aria-hidden="true">
  <div class="pomo-card">
    <div class="pomo-label">FOCUS</div>
    <div class="pomo-title" id="pomoTitle">专注</div>
    <div class="pomo-time" id="pomoTime">25:00</div>
    <div class="pomo-actions">
      <button type="button" id="pomoToggle">开始</button>
      <button type="button" id="pomoRecord">结束并记录</button>
      <button type="button" class="ghost" id="pomoClose">关闭</button>
    </div>
  </div>
</div>

<div class="modal-backdrop" id="reviewModal" role="dialog" aria-modal="true" aria-labelledby="reviewTitle">
  <div class="modal">
    <h2 id="reviewTitle">今日复盘</h2>
    <p class="hint" id="reviewStat">今天完成了 0 项</p>
    <div class="moods" id="moods">
      <button type="button" data-mood="😄">😄 开心</button>
      <button type="button" data-mood="🙂">🙂 一般</button>
      <button type="button" data-mood="😪">😪 疲惫</button>
      <button type="button" data-mood="😢">😢 难过</button>
    </div>
    <textarea id="reviewText" placeholder="今天感觉如何？记一句…"></textarea>
    <div class="actions-row">
      <button type="button" class="cancel" id="reviewCancel">取消</button>
      <button type="button" id="reviewSave">保存</button>
    </div>
  </div>
</div>

<div class="modal-backdrop" id="statsModal" role="dialog" aria-modal="true" aria-labelledby="statsTitle">
  <div class="modal wide">
    <h2 id="statsTitle">打卡</h2>
    <p class="hint" id="statsHint"></p>
    <div class="heat" id="heat"></div>
    <div class="heat-legend"><span>少</span><i></i><i data-l="2"></i><i data-l="4"></i><span>多</span></div>
    <div class="actions-row" style="margin-top:12px">
      <button type="button" id="statsClose">知道了</button>
    </div>
  </div>
</div>

<div class="modal-backdrop" id="reminder" role="dialog" aria-modal="true" aria-labelledby="remindTitle">
  <div class="modal">
    <h2 id="remindTitle">明天到期</h2>
    <p class="hint">下面这些任务明天就到截止日了，记得收尾。</p>
    <ul id="remindList"></ul>
    <div class="actions-row"><button type="button" id="remindClose">知道了</button></div>
  </div>
</div>

<script>
(function () {
  "use strict";
  var PRIORITY_TEXT = { 1: "低", 2: "中", 3: "高" };
  var SORT_TEXT = { id: "编号", priority: "轻重", date: "日期" };
  var state = { priority: 2, sort: "id", dir: "asc", tag: "" };
  var $ = function (id) { return document.getElementById(id); };
  var tasksEl = $("tasks"), statusEl = $("status"), countEl = $("count");

  function pad(n) { return String(n).padStart(2, "0"); }
  function fmt(d) { return d.getFullYear() + "-" + pad(d.getMonth() + 1) + "-" + pad(d.getDate()); }
  function today() { return fmt(new Date()); }
  function tomorrow() { var d = new Date(); d.setDate(d.getDate() + 1); return fmt(d); }
  function addDays(n) { var d = new Date(); d.setDate(d.getDate() + n); return fmt(d); }

  function api(path) {
    return fetch(path).then(function (res) {
      return res.json().then(function (data) {
        if (!res.ok) throw new Error(data.error || "请求失败");
        return data;
      });
    });
  }

  function setStatus(text, kind) {
    statusEl.textContent = text || "";
    statusEl.className = "status" + (kind ? " " + kind : "");
  }

  function el(tag, className, text) {
    var node = document.createElement(tag);
    if (className) node.className = className;
    if (text !== undefined) node.textContent = text;
    return node;
  }

  /* ---------- 一句话识别 ---------- */
  function parseQuick(text) {
    var rest = text;
    var due = "";
    var priority = null;

    var wordDays = [["大后天", 3], ["后天", 2], ["明天", 1], ["今天", 0]];
    for (var i = 0; i < wordDays.length; i++) {
      if (rest.indexOf(wordDays[i][0]) >= 0) {
        due = addDays(wordDays[i][1]);
        rest = rest.replace(wordDays[i][0], " ");
        break;
      }
    }
    if (!due) {
      var m = rest.match(/(\d+)\s*天后/);
      if (m) { due = addDays(Number(m[1])); rest = rest.replace(m[0], " "); }
    }
    if (!due) {
      var iso = rest.match(/(\d{4})-(\d{1,2})-(\d{1,2})/);
      if (iso) {
        due = iso[1] + "-" + pad(Number(iso[2])) + "-" + pad(Number(iso[3]));
        rest = rest.replace(iso[0], " ");
      }
    }

    var p = rest.match(/[!！]{1,2}\s*(高|中|低)?/);
    if (p) {
      var word = p[1];
      priority = word === "中" ? 2 : word === "低" ? 1 : 3;
      rest = rest.replace(p[0], " ");
    } else {
      var pw = rest.match(/(高优先|中优先|低优先)/);
      if (pw) {
        priority = pw[1].charAt(0) === "高" ? 3 : pw[1].charAt(0) === "中" ? 2 : 1;
        rest = rest.replace(pw[0], " ");
      }
    }

    var title = rest.replace(/\s+/g, " ").replace(/^[\s,，。;；]+|[\s,，。;；]+$/g, "");
    return { title: title, due: due, priority: priority };
  }

  /* ---------- 搜索高亮 / 提示条 ---------- */
  var currentQuery = "";
  var undoAction = null;
  var toastTimer = null;

  function highlight(text, needle) {
    if (!needle) return document.createTextNode(text);
    var lowerText = text.toLowerCase();
    var lowerNeedle = needle.toLowerCase();
    var frag = document.createDocumentFragment();
    var from = 0;
    while (from < text.length) {
      var at = lowerText.indexOf(lowerNeedle, from);
      if (at < 0) break;
      if (at > from) frag.appendChild(document.createTextNode(text.slice(from, at)));
      var mark = document.createElement("mark");
      mark.textContent = text.slice(at, at + lowerNeedle.length);
      frag.appendChild(mark);
      from = at + lowerNeedle.length;
    }
    if (from < text.length) frag.appendChild(document.createTextNode(text.slice(from)));
    return frag;
  }

  function hideToast() {
    $("toast").classList.remove("show");
    undoAction = null;
  }

  function showToast(message, onUndo) {
    $("toastText").textContent = message;
    $("toastUndo").hidden = !onUndo;
    undoAction = onUndo || null;
    $("toast").classList.add("show");
    clearTimeout(toastTimer);
    toastTimer = setTimeout(hideToast, 5000);
  }

  $("toastUndo").addEventListener("click", function () {
    var action = undoAction;
    hideToast();
    if (action) action();
  });

  /* ---------- 子任务 ---------- */
  var expandedSubs = {};

  function renderSubs(task) {
    var box = el("div", "subs");

    task.subs.forEach(function (sub, index) {
      var row = el("div", "sub");
      var label = el("label", "subitem" + (sub.done ? " done" : ""));

      var tick = document.createElement("input");
      tick.type = "checkbox";
      tick.checked = sub.done;
      tick.addEventListener("change", function () {
        api("/api/subtoggle?id=" + task.id + "&index=" + index)
          .then(function () { return refresh(); })
          .catch(function (err) { setStatus(err.message, "err"); });
      });
      label.appendChild(tick);
      label.appendChild(el("span", "", sub.title));
      row.appendChild(label);

      var kill = el("button", "subdel", "×");
      kill.type = "button";
      kill.title = "删除子任务";
      kill.addEventListener("click", function () {
        api("/api/subremove?id=" + task.id + "&index=" + index)
          .then(function () { return refresh(); })
          .catch(function (err) { setStatus(err.message, "err"); });
      });
      row.appendChild(kill);
      box.appendChild(row);
    });

    var form = document.createElement("form");
    form.className = "subadd";
    var input = document.createElement("input");
    input.type = "text";
    input.placeholder = "添加子任务…";
    input.maxLength = 120;
    form.appendChild(input);
    form.addEventListener("submit", function (event) {
      event.preventDefault();
      var value = input.value.trim();
      if (!value) return;
      expandedSubs[task.id] = true;
      api("/api/subadd?id=" + task.id + "&title=" + encodeURIComponent(value))
        .then(function () { return refresh(); })
        .catch(function (err) { setStatus(err.message, "err"); });
    });
    box.appendChild(form);
    return box;
  }

  function showParsed(parsed) {
    var bits = [];
    if (parsed.due) bits.push("截止 <b>" + parsed.due + "</b>");
    if (parsed.priority) bits.push("优先级 <b>" + PRIORITY_TEXT[parsed.priority] + "</b>");
    var box = $("parsed");
    if (!bits.length) { box.className = "parsed"; box.innerHTML = ""; return; }
    box.innerHTML = "已识别：" + bits.join(" · ");
    box.className = "parsed show";
  }

  function applyParsed(parsed) {
    if (parsed.due) { $("due").value = parsed.due; syncDateStyle($("due")); }
    if (parsed.priority) setPriority(parsed.priority);
  }

  /* ---------- 展开/收起 ---------- */
  function openDetails(open) {
    $("details").classList.toggle("open", !!open);
  }
  function detailsOpen() { return $("details").classList.contains("open"); }

  $("title").addEventListener("focus", function () { openDetails(true); });
  $("title").addEventListener("input", function () {
    $("hint").classList.remove("show");
    $("title").classList.remove("bad");
    if (this.value.trim()) openDetails(true);
    var parsed = parseQuick(this.value);
    showParsed(parsed);
    applyParsed(parsed);
    syncDateStyle($("due"));
  });

  $("moreToggle").addEventListener("click", function () {
    var field = $("targetField");
    field.hidden = !field.hidden;
    this.textContent = field.hidden ? "＋ 目标完成日" : "－ 收起目标完成日";
    openDetails(true);
  });

  function setPriority(value) {
    state.priority = value;
    Array.prototype.forEach.call($("priority").querySelectorAll("button"), function (btn) {
      btn.setAttribute("aria-pressed", String(Number(btn.dataset.v) === value));
    });
  }

  /* ---------- 列表 ---------- */
  function emptyArt() {
    var NS = "http://www.w3.org/2000/svg";
    var svg = document.createElementNS(NS, "svg");
    svg.setAttribute("width", "104");
    svg.setAttribute("height", "74");
    svg.setAttribute("viewBox", "0 0 132 92");
    svg.setAttribute("fill", "none");
    svg.setAttribute("stroke", "currentColor");
    svg.setAttribute("stroke-width", "1.8");
    svg.setAttribute("stroke-linecap", "round");
    svg.setAttribute("stroke-linejoin", "round");
    [
      "M14 12h72a6 6 0 0 1 6 6v58a6 6 0 0 1-6 6H14a6 6 0 0 1-6-6V18a6 6 0 0 1 6-6z",
      "M20 28h44", "M20 40h56", "M20 52h34", "M20 64h48",
      "M104 20l14 14-40 40-16 2 2-16z", "M104 20l14 14"
    ].forEach(function (d) {
      var path = document.createElementNS(NS, "path");
      path.setAttribute("d", d);
      svg.appendChild(path);
    });
    return svg;
  }

  function row(task) {
    var li = el("li", task.done ? "done" : "");

    var bar = el("span", "prio-bar p" + task.priority);
    bar.title = "优先级：" + (PRIORITY_TEXT[task.priority] || "中");
    li.appendChild(bar);

    var check = el("button", "check", "✓");
    check.type = "button";
    check.title = task.done ? "取消完成" : "标为完成";
    check.setAttribute("aria-label", check.title);
    check.addEventListener("click", function () {
      if (task.done) {
        mutate("/api/done?id=" + task.id);
        return;
      }
      li.classList.add("done"); // 先划掉，再写入
      setTimeout(function () { mutate("/api/done?id=" + task.id); }, 420);
    });
    li.appendChild(check);

    var body = el("div", "body");
    var top = el("div", "row-top");
    var titleEl = el("div", "title");
    if (task.sticker) {
      var sticker = el("span", "sticker-mark", task.sticker);
      titleEl.appendChild(sticker);
    }
    titleEl.appendChild(highlight(task.title, currentQuery));
    titleEl.title = "点击查看详情";
    titleEl.style.cursor = "pointer";
    titleEl.addEventListener("click", function () { openDrawer(task); });
    top.appendChild(titleEl);

    if (task.due) {
      var late = !task.done && task.due < today();
      var due = el("span", "due" + (late ? " late" : ""), (late ? "⚠ " : "") + task.due);
      due.title = late ? "已逾期" : "截止日期";
      top.appendChild(due);
    }
    body.appendChild(top);

    var meta = el("div", "meta");
    if (task.repeat) {
      var repeatText = task.repeat === "daily" ? "每天" : task.repeat === "weekly" ? "每周" : "每月";
      meta.appendChild(el("span", "", "↻ " + repeatText));
    }
    if (task.spent > 0) {
      meta.appendChild(el("span", "", "⏱ " + Math.max(1, Math.round(task.spent / 60)) + " 分"));
    }
    (task.tags || []).forEach(function (tag) {
      meta.appendChild(el("span", "tag", "#" + tag));
    });
    if (task.target) meta.appendChild(el("span", "", "🎯 " + task.target));
    meta.appendChild(el("span", "", "#" + task.id));

    var subBtn = el("button", "subbtn",
                    task.subTotal ? "☑ " + task.subDone + "/" + task.subTotal : "＋ 子任务");
    subBtn.type = "button";
    subBtn.title = "子任务清单";
    subBtn.addEventListener("click", function () {
      expandedSubs[task.id] = !expandedSubs[task.id];
      refresh().catch(function (err) { setStatus(err.message, "err"); });
    });
    meta.appendChild(subBtn);
    body.appendChild(meta);

    if (expandedSubs[task.id]) body.appendChild(renderSubs(task));
    li.appendChild(body);

    var actions = el("div", "actions");
    var del = el("button", "", "划掉");
    del.type = "button";
    del.addEventListener("click", function () {
      api("/api/remove?id=" + task.id).then(function () {
        return refresh();
      }).then(function () {
        showToast("已删除「" + task.title + "」", function () {
          api("/api/undo").then(function () { return refresh(); })
            .catch(function (err) { setStatus(err.message, "err"); });
        });
      }).catch(function (err) { setStatus(err.message, "err"); });
    });
    actions.appendChild(del);
    li.appendChild(actions);
    return li;
  }

  function render(tasks, freshId) {
    tasksEl.innerHTML = "";
    countEl.textContent = tasks.length ? "共 " + tasks.length + " 条" : "";

    if (!tasks.length) {
      var empty = el("div", "empty");
      empty.appendChild(emptyArt());
      var query = $("search").value.trim();
      if (query) {
        empty.appendChild(el("div", "art", "没有找到相关的任务，换个词试试？"));
        var clear = el("button", "", "清除筛选");
        clear.type = "button";
        clear.addEventListener("click", function () {
          $("search").value = "";
          refresh().catch(function (err) { setStatus(err.message, "err"); });
        });
        empty.appendChild(clear);
      } else {
        empty.appendChild(el("div", "art", "这一页还空着"));
      }
      tasksEl.appendChild(empty);
      return;
    }

    tasks.forEach(function (task) {
      var node = row(task);
      if (freshId && task.id === freshId) node.classList.add("fresh");
      tasksEl.appendChild(node);
    });
  }

  function sortButtons() {
    Array.prototype.forEach.call($("sort").querySelectorAll("button"), function (btn) {
      var active = btn.dataset.sort === state.sort;
      btn.setAttribute("aria-pressed", String(active));
      btn.textContent = SORT_TEXT[btn.dataset.sort] + (active ? (state.dir === "asc" ? " ↑" : " ↓") : "");
    });
  }

  function refresh(freshId) {
    var q = $("search").value.trim();
    currentQuery = q;
    return api("/api/tasks?sort=" + encodeURIComponent(state.sort) + "&q=" + encodeURIComponent(q))
      .then(function (data) {
        var list = data.tasks;
        if (state.tag) {
          list = list.filter(function (task) {
            return (task.tags || []).indexOf(state.tag) >= 0;
          });
        }
        if (state.dir === "desc") list = list.slice().reverse();
        render(list, freshId);
        updateFilterLine();
        setStatus(q ? "找到 " + list.length + " 条" : "");
        if (!q) maybeRemind(data.tasks);

        // 全部勾完时，自动弹出今日复盘
        var allDone = list.length > 0 && list.every(function (task) { return task.done; });
        if (lastAllDone === false && allDone) {
          setTimeout(openReview, 450);
        }
        lastAllDone = allDone;
      });
  }

  function mutate(path) {
    return api(path).then(function () { return refresh(); })
      .catch(function (err) { setStatus(err.message, "err"); });
  }

  /* ---------- 表单 ---------- */
  $("addForm").addEventListener("submit", function (event) {
    event.preventDefault();
    var raw = $("title").value.trim();
    if (!raw) {
      $("hint").classList.add("show");
      $("title").classList.add("bad");
      $("title").classList.remove("shake");
      void $("title").offsetWidth;
      $("title").classList.add("shake");
      $("title").focus();
      return;
    }

    var parsed = parseQuick(raw);
    var title = parsed.title || raw;
    var priority = parsed.priority || state.priority;
    var due = parsed.due || $("due").value || today();
    var target = $("targetField").hidden ? "" : $("target").value;
    if (target && target > due) {
      setStatus("目标完成日不能晚于截止日", "err");
      return;
    }

    var button = $("addBtn");
    button.classList.remove("busy");
    void button.offsetWidth;
    button.classList.add("busy");

    var path = "/api/add?title=" + encodeURIComponent(title) +
               "&priority=" + priority +
               "&due=" + encodeURIComponent(due) +
               "&target=" + encodeURIComponent(target) +
               "&repeat=" + encodeURIComponent($("repeat").value) +
               "&tags=" + encodeURIComponent($("tags").value);
    api(path).then(function (data) {
      $("title").value = "";
      $("tags").value = "";
      $("parsed").className = "parsed";
      $("parsed").innerHTML = "";
      openDetails(false);
      return refresh(data.id);
    }).then(function () {
      setStatus("已记下 ✎", "ok");
    }).catch(function (err) { setStatus(err.message, "err"); });
  });

  $("title").addEventListener("keydown", function (event) {
    if (event.key === "Escape") {
      this.value = "";
      openDetails(false);
      $("parsed").className = "parsed";
      this.blur();
    }
  });

  $("priority").addEventListener("click", function (event) {
    var button = event.target.closest("button[data-v]");
    if (!button) return;
    setPriority(Number(button.dataset.v));
  });

  $("sort").addEventListener("click", function (event) {
    var button = event.target.closest("button[data-sort]");
    if (!button) return;
    if (button.dataset.sort === state.sort) {
      state.dir = state.dir === "asc" ? "desc" : "asc"; // 再点一次切换正/倒序
    } else {
      state.sort = button.dataset.sort;
      state.dir = "asc";
    }
    sortButtons();
    refresh().catch(function (err) { setStatus(err.message, "err"); });
  });

  $("search").addEventListener("input", function () {
    refresh().catch(function (err) { setStatus(err.message, "err"); });
  });

  $("clearDone").addEventListener("click", function () {
    api("/api/clear-done").then(function (data) {
      return refresh().then(function () {
        setStatus(data.removed ? "已清空 " + data.removed + " 条已完成任务" : "没有已完成的任务", "ok");
      });
    }).catch(function (err) { setStatus(err.message, "err"); });
  });

  // 快捷键：Ctrl/Cmd+Enter 记下，N 新建，Esc 退出输入（输入框内）/关闭弹窗
  document.addEventListener("keydown", function (event) {
    var tag = (event.target.tagName || "").toLowerCase();
    var typing = tag === "input" || tag === "textarea" || tag === "select";

    if ((event.ctrlKey || event.metaKey) && (event.key === "Enter" || event.keyCode === 10)) {
      event.preventDefault();
      if ($("addForm").requestSubmit) $("addForm").requestSubmit();
      else $("addForm").dispatchEvent(new Event("submit", { cancelable: true }));
      return;
    }
    if (!typing && !event.ctrlKey && !event.metaKey && (event.key === "n" || event.key === "N")) {
      event.preventDefault();
      $("title").focus();
    }
  });

  /* ---------- 任务详情抽屉 ---------- */
  var drawerTask = null;

  function setDrawerStatus(text, isError) {
    var node = $("drawerStatus");
    node.textContent = text || "";
    node.style.color = isError ? "var(--red)" : "";
  }

  function linkify(text) {
    var box = $("links");
    box.innerHTML = "";
    var re = /https?:\/\/[^\s]+/g;
    var seen = {};
    var match;
    while ((match = re.exec(text)) !== null) {
      if (seen[match[0]]) continue;
      seen[match[0]] = true;
      var a = document.createElement("a");
      a.href = match[0];
      a.textContent = match[0];
      a.target = "_blank";
      a.rel = "noreferrer";
      box.appendChild(a);
    }
  }

  function renderFiles(task) {
    var list = $("files");
    list.innerHTML = "";
    (task.files || []).forEach(function (name) {
      var li = document.createElement("li");

      var a = document.createElement("a");
      a.href = "/attach?name=" + encodeURIComponent(name);
      a.textContent = name.replace(/^\d+_/, "");
      a.target = "_blank";
      a.rel = "noreferrer";
      li.appendChild(a);

      var del = document.createElement("button");
      del.type = "button";
      del.textContent = "×";
      del.title = "移除附件";
      del.addEventListener("click", function () {
        api("/api/detach?id=" + task.id + "&name=" + encodeURIComponent(name))
          .then(reloadDrawer)
          .catch(function (err) { setDrawerStatus(err.message, true); });
      });
      li.appendChild(del);
      list.appendChild(li);
    });
  }

  function openDrawer(task) {
    drawerTask = task;
    $("drawerTitle").textContent = task.title;
    $("notes").value = task.notes || "";
    linkify(task.notes || "");
    renderFiles(task);
    renderStickers(task);
    $("spentText").textContent = task.spent > 0
      ? "已专注 " + Math.max(1, Math.round(task.spent / 60)) + " 分钟"
      : "";
    setDrawerStatus("");
    $("drawer").classList.add("show");
    $("drawerBack").classList.add("show");
    $("drawer").setAttribute("aria-hidden", "false");
  }

  function closeDrawer() {
    drawerTask = null;
    $("drawer").classList.remove("show");
    $("drawerBack").classList.remove("show");
    $("drawer").setAttribute("aria-hidden", "true");
  }

  function reloadDrawer() {
    if (!drawerTask) return Promise.resolve();
    var id = drawerTask.id;
    return api("/api/tasks").then(function (data) {
      data.tasks.forEach(function (t) {
        if (t.id === id) {
          drawerTask = t;
          renderFiles(t);
          renderStickers(t);
          $("spentText").textContent = t.spent > 0
            ? "已专注 " + Math.max(1, Math.round(t.spent / 60)) + " 分钟"
            : "";
        }
      });
      return refresh();
    });
  }

  function saveNotes() {
    if (!drawerTask) return;
    api("/api/notes?id=" + drawerTask.id + "&notes=" + encodeURIComponent($("notes").value))
      .then(function () {
        setDrawerStatus("已保存");
        return refresh();
      })
      .catch(function (err) { setDrawerStatus(err.message, true); });
  }

  $("notes").addEventListener("input", function () { linkify(this.value); });
  $("notes").addEventListener("keydown", function (event) {
    if ((event.ctrlKey || event.metaKey) && event.key === "Enter") saveNotes();
  });
  $("saveNotes").addEventListener("click", saveNotes);

  $("fileInput").addEventListener("change", function () {
    if (!drawerTask) return;
    var id = drawerTask.id;
    var picked = Array.prototype.slice.call(this.files || []);
    if (!picked.length) return;

    setDrawerStatus("上传中…");
    var chain = Promise.resolve();
    picked.forEach(function (file) {
      chain = chain.then(function () {
        return fetch("/api/attach?id=" + id + "&name=" + encodeURIComponent(file.name),
                     { method: "POST", body: file });
      });
    });
    chain.then(function () {
      $("fileInput").value = "";
      setDrawerStatus("上传完成");
      return reloadDrawer();
    }).catch(function (err) { setDrawerStatus(err.message, true); });
  });

  $("drawerClose").addEventListener("click", closeDrawer);
  $("drawerBack").addEventListener("click", closeDrawer);

  /* ---------- 贴纸 ---------- */
  var STICKERS = ["🧸", "⭐", "🌱", "☕", "🎯", "🔥", "📌", "🍀"];

  function renderStickers(task) {
    var box = $("stickers");
    box.innerHTML = "";
    STICKERS.forEach(function (emoji) {
      var button = document.createElement("button");
      button.type = "button";
      button.textContent = emoji;
      button.setAttribute("aria-pressed", String(task.sticker === emoji));
      button.addEventListener("click", function () {
        var next = task.sticker === emoji ? "" : emoji;
        api("/api/sticker?id=" + task.id + "&sticker=" + encodeURIComponent(next))
          .then(reloadDrawer)
          .catch(function (err) { setDrawerStatus(err.message, true); });
      });
      box.appendChild(button);
    });
  }

  /* ---------- 专注计时 ---------- */
  var pomo = { id: null, left: 25 * 60, timer: null, running: false };

  function pomoRender() {
    var m = Math.floor(pomo.left / 60);
    var s = pomo.left % 60;
    $("pomoTime").textContent = pad(m) + ":" + pad(s);
  }

  function pomoTick() {
    pomo.left -= 1;
    if (pomo.left <= 0) { pomo.left = 0; pomoStop(); }
    pomoRender();
  }

  function pomoStart() {
    if (pomo.running || pomo.left <= 0) return;
    pomo.running = true;
    $("pomoToggle").textContent = "暂停";
    pomo.timer = setInterval(pomoTick, 1000);
  }

  function pomoStop() {
    pomo.running = false;
    clearInterval(pomo.timer);
    pomo.timer = null;
    $("pomoToggle").textContent = "继续";
  }

  function closePomo() {
    pomoStop();
    $("pomo").classList.remove("show");
  }

  $("startFocus").addEventListener("click", function () {
    if (!drawerTask) return;
    pomo.id = drawerTask.id;
    pomo.left = 25 * 60;
    $("pomoTitle").textContent = drawerTask.title;
    pomoRender();
    $("pomo").classList.add("show");
    pomoStart();
  });

  $("pomoToggle").addEventListener("click", function () {
    if (pomo.running) pomoStop();
    else pomoStart();
  });

  $("pomoClose").addEventListener("click", closePomo);

  $("pomoRecord").addEventListener("click", function () {
    var elapsed = 25 * 60 - pomo.left;
    pomoStop();
    if (!pomo.id || elapsed <= 0) { closePomo(); return; }
    api("/api/spent?id=" + pomo.id + "&seconds=" + elapsed)
      .then(function () {
        setStatus("记录专注 " + Math.max(1, Math.round(elapsed / 60)) + " 分钟", "ok");
        return reloadDrawer();
      })
      .then(closePomo)
      .catch(function (err) { setStatus(err.message, "err"); });
  });

  /* ---------- 回收站 ---------- */
  function closeTrash() {
    $("trashDrawer").classList.remove("show");
    $("trashBack").classList.remove("show");
    $("trashDrawer").setAttribute("aria-hidden", "true");
  }

  function openTrash() {
    api("/api/trash").then(function (data) {
      var list = $("trashList");
      list.innerHTML = "";
      if (!data.items.length) {
        var empty = document.createElement("li");
        empty.className = "empty-line";
        empty.textContent = "回收站是空的";
        list.appendChild(empty);
      }
      data.items.forEach(function (item, index) {
        var li = document.createElement("li");
        li.appendChild(el("span", "t-title", item.title));
        li.appendChild(el("span", "t-meta",
                           (PRIORITY_TEXT[item.priority] || "中") + (item.due ? " · " + item.due : "")));

        var restore = el("button", "", "恢复");
        restore.type = "button";
        restore.addEventListener("click", function () {
          api("/api/restore?index=" + index)
            .then(function () { return refresh(); })
            .then(openTrash)
            .catch(function (err) { setStatus(err.message, "err"); });
        });
        li.appendChild(restore);
        list.appendChild(li);
      });
      $("trashDrawer").classList.add("show");
      $("trashBack").classList.add("show");
      $("trashDrawer").setAttribute("aria-hidden", "false");
    }).catch(function (err) { setStatus(err.message, "err"); });
  }

  $("openTrash").addEventListener("click", openTrash);
  $("trashClose").addEventListener("click", closeTrash);
  $("trashBack").addEventListener("click", closeTrash);

  /* ---------- 标签 ---------- */
  function updateFilterLine() {
    var line = $("filterLine");
    if (state.tag) {
      line.hidden = false;
      $("tagFilter").textContent = "#" + state.tag + " ✕";
      $("tagFilter").title = "清除标签筛选";
    } else {
      line.hidden = true;
    }
  }

  $("tagFilter").addEventListener("click", function () {
    state.tag = "";
    refresh().catch(function (err) { setStatus(err.message, "err"); });
  });

  function closeTags() {
    $("tagDrawer").classList.remove("show");
    $("tagBack").classList.remove("show");
    $("tagDrawer").setAttribute("aria-hidden", "true");
  }

  function openTags() {
    api("/api/tasks").then(function (data) {
      var counts = {};
      data.tasks.forEach(function (task) {
        (task.tags || []).forEach(function (tag) {
          counts[tag] = (counts[tag] || 0) + 1;
        });
      });

      var list = $("tagList");
      list.innerHTML = "";
      var names = Object.keys(counts).sort();
      if (!names.length) {
        var empty = document.createElement("li");
        empty.className = "empty-line";
        empty.textContent = "还没有标签，添加任务时在「标签」里写上 工作 生活";
        list.appendChild(empty);
      }
      names.forEach(function (tag) {
        var li = document.createElement("li");
        var btn = document.createElement("button");
        btn.type = "button";
        btn.textContent = "#" + tag;
        btn.addEventListener("click", function () {
          state.tag = tag;
          closeTags();
          refresh().catch(function (err) { setStatus(err.message, "err"); });
        });
        li.appendChild(btn);
        li.appendChild(el("span", "tagcount", counts[tag] + " 条"));
        list.appendChild(li);
      });

      $("tagDrawer").classList.add("show");
      $("tagBack").classList.add("show");
      $("tagDrawer").setAttribute("aria-hidden", "false");
    }).catch(function (err) { setStatus(err.message, "err"); });
  }

  $("openTags").addEventListener("click", openTags);
  $("tagClose").addEventListener("click", closeTags);
  $("tagBack").addEventListener("click", closeTags);

  /* ---------- 复盘 / 打卡 ---------- */
  var lastStats = null;
  var lastAllDone = null;
  var moodValue = "";

  function setMood(value) {
    moodValue = value || "";
    Array.prototype.forEach.call($("moods").querySelectorAll("button"), function (btn) {
      btn.setAttribute("aria-pressed", String(btn.dataset.mood === moodValue));
    });
  }

  $("moods").addEventListener("click", function (event) {
    var button = event.target.closest("button[data-mood]");
    if (!button) return;
    setMood(moodValue === button.dataset.mood ? "" : button.dataset.mood);
  });

  function openReview() {
    api("/api/stats").then(function (stats) {
      lastStats = stats;
      var doneToday = stats.days.length ? stats.days[stats.days.length - 1].count : 0;
      $("reviewStat").textContent = "今天完成 " + doneToday + " 项 · 累计 " + stats.done + "/" + stats.total;
      $("reviewText").value = "";
      setMood("");
      $("reviewModal").classList.add("show");
      return api("/api/reviews").then(function (data) {
        data.items.forEach(function (item) {
          if (item.date === stats.today) {
            $("reviewText").value = item.text;
            setMood(item.mood);
          }
        });
      });
    }).catch(function (err) { setStatus(err.message, "err"); });
  }

  $("openReview").addEventListener("click", openReview);
  $("reviewCancel").addEventListener("click", function () {
    $("reviewModal").classList.remove("show");
  });
  $("reviewModal").addEventListener("click", function (event) {
    if (event.target === this) this.classList.remove("show");
  });
  $("reviewSave").addEventListener("click", function () {
    var date = lastStats ? lastStats.today : today();
    var text = $("reviewText").value.trim();
    api("/api/review?date=" + encodeURIComponent(date) +
        "&mood=" + encodeURIComponent(moodValue) +
        "&text=" + encodeURIComponent(text))
      .then(function () {
        $("reviewModal").classList.remove("show");
        setStatus(text || moodValue ? "已记下今日回顾" : "已清除今日回顾", "ok");
      })
      .catch(function (err) { setStatus(err.message, "err"); });
  });

  function openStats() {
    api("/api/stats").then(function (stats) {
      lastStats = stats;
      var heat = $("heat");
      heat.innerHTML = "";

      var first = new Date(stats.days[0].date + "T00:00:00");
      var pad = (first.getDay() + 6) % 7; // 让每列从周一开始
      for (var p = 0; p < pad; p++) {
        var blank = document.createElement("i");
        blank.className = "blank";
        heat.appendChild(blank);
      }

      stats.days.forEach(function (day) {
        var cell = document.createElement("i");
        var level = day.count === 0 ? 0 : day.count === 1 ? 1 : day.count === 2 ? 2 :
                    day.count <= 4 ? 3 : 4;
        if (level) cell.setAttribute("data-l", String(level));
        cell.title = day.date + " · " + day.count + " 项";
        heat.appendChild(cell);
      });

      $("statsHint").textContent = "最近 12 周 · 累计完成 " + stats.done + " / " + stats.total + " 项";
      $("statsModal").classList.add("show");
    }).catch(function (err) { setStatus(err.message, "err"); });
  }

  $("openStats").addEventListener("click", openStats);
  $("statsClose").addEventListener("click", function () {
    $("statsModal").classList.remove("show");
  });
  $("statsModal").addEventListener("click", function (event) {
    if (event.target === this) this.classList.remove("show");
  });
  $("pomo").addEventListener("click", function (event) {
    if (event.target === this) closePomo();
  });

  /* ---------- 提醒 ---------- */
  var reminded = false;
  function maybeRemind(tasks) {
    if (reminded) return;
    var soon = tasks.filter(function (task) {
      return !task.done && task.due && task.due === tomorrow();
    });
    if (!soon.length) return;
    reminded = true;

    var list = $("remindList");
    list.innerHTML = "";
    soon.forEach(function (task) {
      var li = document.createElement("li");
      var when = el("span", "when", "明天 · " + task.due);
      li.appendChild(when);
      li.appendChild(el("span", "", task.title));
      list.appendChild(li);
    });
    $("reminder").classList.add("show");
  }

  function closeReminder() { $("reminder").classList.remove("show"); }
  $("remindClose").addEventListener("click", closeReminder);
  $("reminder").addEventListener("click", function (event) {
    if (event.target === this) closeReminder();
  });
  document.addEventListener("keydown", function (event) {
    if (event.key !== "Escape") return;
    closeReminder();
    closeDrawer();
    closeTrash();
    closeTags();
    closePomo();
    $("reviewModal").classList.remove("show");
    $("statsModal").classList.remove("show");
  });

  /* ---------- 日期样式 ---------- */
  function syncDateStyle(input) {
    input.classList.toggle("filled", !!input.value);
  }
  ["due", "target"].forEach(function (id) {
    var input = $(id);
    input.addEventListener("input", function () { syncDateStyle(input); });
    input.addEventListener("change", function () { syncDateStyle(input); });
  });

  /* ---------- 弹窗拖动 ---------- */
  function makeDraggable(card) {
    if (!card) return;
    var drag = { on: false, sx: 0, sy: 0, tx: 0, ty: 0 };

    card.addEventListener("pointerdown", function (event) {
      if (event.button !== 0) return;
      if (event.target.closest("input, textarea, select, button, a, label")) return;
      drag.on = true;
      drag.sx = event.clientX;
      drag.sy = event.clientY;
      drag.tx = parseFloat(card.style.getPropertyValue("--tx")) || 0;
      drag.ty = parseFloat(card.style.getPropertyValue("--ty")) || 0;
      card.classList.add("dragging");
      try { card.setPointerCapture(event.pointerId); } catch (err) {}
      event.preventDefault();
    });

    card.addEventListener("pointermove", function (event) {
      if (!drag.on) return;
      card.style.setProperty("--tx", (drag.tx + event.clientX - drag.sx) + "px");
      card.style.setProperty("--ty", (drag.ty + event.clientY - drag.sy) + "px");
    });

    function stop(event) {
      if (!drag.on) return;
      drag.on = false;
      card.classList.remove("dragging");
      try { card.releasePointerCapture(event.pointerId); } catch (err) {}
    }
    card.addEventListener("pointerup", stop);
    card.addEventListener("pointercancel", stop);
  }

  ["reminder", "reviewModal", "statsModal", "pomo"].forEach(function (id) {
    var box = $(id);
    if (!box) return;
    makeDraggable(box.querySelector(".modal") || box.querySelector(".pomo-card"));
  });

  var now = new Date();
  $("noteDate").textContent = String(now.getMonth() + 1) + "/" + String(now.getDate());
  $("due").value = today();
  syncDateStyle($("due"));
  syncDateStyle($("target"));
  sortButtons();
  refresh().catch(function (err) { setStatus(err.message, "err"); });
})();
</script>
</body>
</html>
)HTML";

// ---------- 字符串与 JSON 工具 ----------

std::string toLower(std::string text) {
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

std::string jsonEscape(const std::string& text) {
    std::ostringstream out;
    for (unsigned char c : text) {
        switch (c) {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (c < 0x20) {
                char buffer[7];
                std::snprintf(buffer, sizeof(buffer), "\\u%04x", c);
                out << buffer;
            } else {
                out << static_cast<char>(c);
            }
        }
    }
    return out.str();
}

std::string urlDecode(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '+') {
            out.push_back(' ');
        } else if (c == '%' && i + 2 < text.size() &&
                   std::isxdigit(static_cast<unsigned char>(text[i + 1])) &&
                   std::isxdigit(static_cast<unsigned char>(text[i + 2]))) {
            const std::string hex = text.substr(i + 1, 2);
            out.push_back(static_cast<char>(std::stoi(hex, nullptr, 16)));
            i += 2;
        } else {
            out.push_back(c);
        }
    }
    return out;
}

using Query = std::vector<std::pair<std::string, std::string>>;

Query parseQuery(const std::string& text) {
    Query query;
    std::size_t start = 0;
    while (start <= text.size()) {
        std::size_t end = text.find('&', start);
        if (end == std::string::npos) end = text.size();
        const std::string item = text.substr(start, end - start);
        const std::size_t eq = item.find('=');
        if (eq != std::string::npos) {
            query.emplace_back(urlDecode(item.substr(0, eq)), urlDecode(item.substr(eq + 1)));
        } else if (!item.empty()) {
            query.emplace_back(urlDecode(item), std::string());
        }
        if (end == text.size()) break;
        start = end + 1;
    }
    return query;
}

std::string queryValue(const Query& query, const std::string& key, const std::string& fallback = "") {
    for (const auto& item : query) {
        if (item.first == key) return item.second;
    }
    return fallback;
}

bool parseInt(const std::string& text, int& value) {    if (text.empty()) return false;
    try {
        std::size_t used = 0;
        const int parsed = std::stoi(text, &used);
        if (used != text.size()) return false;
        value = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

// 网页接口里 1/2/3 直接就是枚举值（1=低、2=中、3=高），
// 也接受「高/中/低」这类名字，和命令行保持一致。
bool priorityFromValue(const std::string& text, Priority& out) {
    int number = 0;
    if (parseInt(text, number)) {
        if (number < 1 || number > 3) return false;
        out = static_cast<Priority>(number);
        return true;
    }
    return parsePriority(text, out);
}

std::string tasksToJson(const std::vector<Task>& tasks) {
    std::ostringstream out;
    out << "{\"count\":" << tasks.size() << ",\"tasks\":[";
    for (std::size_t i = 0; i < tasks.size(); ++i) {
        const Task& task = tasks[i];
        if (i) out << ',';
        out << "{\"id\":" << task.id
            << ",\"title\":\"" << jsonEscape(task.title) << "\""
            << ",\"done\":" << (task.done ? "true" : "false")
            << ",\"priority\":" << static_cast<int>(task.priority)
            << ",\"due\":\"" << jsonEscape(task.due) << "\""
            << ",\"target\":\"" << jsonEscape(task.target) << "\""
            << ",\"subs\":[";
        std::size_t subDone = 0;
        for (std::size_t s = 0; s < task.subs.size(); ++s) {
            if (s) out << ',';
            if (task.subs[s].done) ++subDone;
            out << "{\"title\":\"" << jsonEscape(task.subs[s].title) << "\""
                << ",\"done\":" << (task.subs[s].done ? "true" : "false") << "}";
        }
        out << "],\"subTotal\":" << task.subs.size()
            << ",\"subDone\":" << subDone
            << ",\"notes\":\"" << jsonEscape(task.notes) << "\""
            << ",\"repeat\":\"" << jsonEscape(task.repeat) << "\""
            << ",\"doneAt\":\"" << jsonEscape(task.doneAt) << "\""
            << ",\"spent\":" << task.spent
            << ",\"sticker\":\"" << jsonEscape(task.sticker) << "\""
            << ",\"tags\":[";
        for (std::size_t t = 0; t < task.tags.size(); ++t) {
            if (t) out << ',';
            out << "\"" << jsonEscape(task.tags[t]) << "\"";
        }
        out << "],\"files\":[";
        for (std::size_t f = 0; f < task.files.size(); ++f) {
            if (f) out << ',';
            out << "\"" << jsonEscape(task.files[f]) << "\"";
        }
        out << "]}";
    }
    out << "]}";
    return out.str();
}

std::string errorJson(const std::string& message) {
    return "{\"error\":\"" + jsonEscape(message) + "\"}";
}

// ---------- HTTP ----------

std::string readRequestText(SOCKET client) {
    std::string request;
    char buffer[4096];
    while (request.find("\r\n\r\n") == std::string::npos && request.size() < 65536) {
        const int received = recv(client, buffer, sizeof(buffer), 0);
        if (received <= 0) break;
        request.append(buffer, static_cast<std::size_t>(received));
    }
    return request;
}

void writeAll(SOCKET client, const std::string& text) {
    std::size_t sent = 0;
    while (sent < text.size()) {
        const int chunk = send(client, text.data() + sent,
                               static_cast<int>(text.size() - sent), 0);
        if (chunk <= 0) return;
        sent += static_cast<std::size_t>(chunk);
    }
}

void respond(SOCKET client, int status, const char* statusText, const char* contentType,
             const std::string& body, const std::string& extraHeaders = "") {
    std::ostringstream head;
    head << "HTTP/1.1 " << status << " " << statusText << "\r\n"
         << "Content-Type: " << contentType << "; charset=utf-8\r\n";
    if (!extraHeaders.empty()) head << extraHeaders;
    head << "Content-Length: " << body.size() << "\r\n"
         << "Cache-Control: no-store\r\n"
         << "Connection: close\r\n\r\n";
    writeAll(client, head.str() + body);
}

// 数据文件放在可执行文件旁边，和命令行版共用同一份 tasks.txt。
std::string defaultDataPath(const char* argv0) {
    const std::string exe = argv0 ? argv0 : "";
    const std::size_t slash = exe.find_last_of("\\/");
    if (slash == std::string::npos) return "tasks.txt";
    return exe.substr(0, slash + 1) + "tasks.txt";
}

std::string dirOf(const std::string& path) {
    const std::size_t slash = path.find_last_of("\\/");
    return slash == std::string::npos ? std::string(".") : path.substr(0, slash);
}

std::string attachmentsDir(const std::string& dataPath) {
    return dirOf(dataPath) + "\\attachments";
}

// 只保留能安全落盘的文件名字符，挡掉路径分隔符和 ..
std::string sanitizeName(const std::string& name) {
    std::string out;
    for (char c : name) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (u < 0x20) continue;
        if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' ||
            c == '"' || c == '<' || c == '>' || c == '|') continue;
        out.push_back(c);
    }
    if (out.empty() || out == "." || out == "..") out = "file";
    return out;
}

std::size_t contentLengthOf(const std::string& request) {
    const std::string key = "Content-Length:";
    std::size_t at = request.find(key);
    if (at == std::string::npos) return 0;
    at += key.size();
    while (at < request.size() && (request[at] == ' ' || request[at] == '\t')) ++at;
    std::size_t end = at;
    while (end < request.size() && std::isdigit(static_cast<unsigned char>(request[end]))) ++end;
    if (end == at) return 0;
    return static_cast<std::size_t>(std::stoull(request.substr(at, end - at)));
}

std::string readBody(SOCKET client, const std::string& request, std::size_t length) {
    std::string body;
    const std::size_t headEnd = request.find("\r\n\r\n");
    if (headEnd != std::string::npos) body = request.substr(headEnd + 4);
    while (body.size() < length) {
        char buffer[8192];
        const int received = recv(client, buffer, sizeof(buffer), 0);
        if (received <= 0) break;
        body.append(buffer, static_cast<std::size_t>(received));
    }
    if (body.size() > length) body.resize(length);
    return body;
}

bool writeFileBytes(const std::string& path, const std::string& data) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(out);
}

bool readFileBytes(const std::string& path, std::string& data) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream buffer;
    buffer << in.rdbuf();
    data = buffer.str();
    return true;
}

// 本地日期字符串，offsetDays 可前后偏移。
std::string localDateString(int offsetDays = 0) {
    std::time_t now = std::time(nullptr);
    now += static_cast<std::time_t>(offsetDays) * 24 * 60 * 60;
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char buffer[11] = {};
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d",
                  local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
    return buffer;
}

std::string reviewsPath(const std::string& dataPath) {
    return dirOf(dataPath) + "\\reviews.txt";
}

std::string escapeLine(const std::string& text) {
    std::string out;
    for (char c : text) {
        if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c != '\r') out.push_back(c);
    }
    return out;
}

std::string unescapeLine(const std::string& text) {
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\\' && i + 1 < text.size()) {
            if (text[i + 1] == 'n') { out.push_back('\n'); ++i; continue; }
            if (text[i + 1] == '\\') { out.push_back('\\'); ++i; continue; }
        }
        out.push_back(text[i]);
    }
    return out;
}

struct Review {
    std::string date;
    std::string mood;
    std::string text;
};

// 读出全部复盘记录：日期 / 心情 / 感想
std::vector<Review> loadReviews(const std::string& path) {
    std::vector<Review> items;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        std::vector<std::string> parts;
        std::size_t start = 0;
        while (true) {
            const std::size_t tab = line.find('\t', start);
            if (tab == std::string::npos) { parts.push_back(line.substr(start)); break; }
            parts.push_back(line.substr(start, tab - start));
            start = tab + 1;
        }
        if (parts.empty()) continue;
        Review item;
        item.date = parts[0];
        if (parts.size() >= 3) {
            item.mood = parts[1];
            item.text = unescapeLine(parts[2]);
        } else if (parts.size() == 2) {
            item.text = unescapeLine(parts[1]); // 旧格式：只有日期和感想
        }
        items.push_back(item);
    }
    return items;
}

bool saveReviews(const std::string& path, const std::vector<Review>& items) {
    std::ofstream out(path);
    if (!out) return false;
    for (const Review& item : items) {
        out << item.date << "\t" << item.mood << "\t" << escapeLine(item.text) << "\n";
    }
    return static_cast<bool>(out);
}

// 重复任务的下一次日期：daily +1 天，weekly +7 天，monthly +1 个月（月末自动收敛）
int daysInMonth(int year, int month) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2) {
        const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
        return leap ? 29 : 28;
    }
    return days[month - 1];
}

std::string shiftDate(const std::string& date, const std::string& repeat) {
    if (date.size() != 10 || repeat.empty()) return date;
    int y = std::stoi(date.substr(0, 4));
    int m = std::stoi(date.substr(5, 2));
    int d = std::stoi(date.substr(8, 2));

    if (repeat == "monthly") {
        m += 1;
        if (m > 12) { m = 1; y += 1; }
        d = std::min(d, daysInMonth(y, m));
    } else if (repeat == "weekly") {
        d += 7;
    } else if (repeat == "daily") {
        d += 1;
    } else {
        return date;
    }

    while (d > daysInMonth(y, m)) {
        d -= daysInMonth(y, m);
        m += 1;
        if (m > 12) { m = 1; y += 1; }
    }

    char buffer[11] = {};
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", y, m, d);
    return buffer;
}

const char* mimeOf(const std::string& name) {    const std::size_t dot = name.find_last_of('.');
    std::string ext = dot == std::string::npos ? "" : name.substr(dot);
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".pdf") return "application/pdf";
    if (ext == ".txt" || ext == ".md") return "text/plain";
    return "application/octet-stream";
}

} // namespace

int main(int argc, char* argv[]) {
    int port = 8080;
    if (argc > 1) parseInt(argv[1], port);

    const std::string dataPath = defaultDataPath(argc > 0 ? argv[0] : nullptr);
    TaskManager manager;
    if (manager.load(dataPath)) {
        std::cout << "Loaded " << manager.size() << " task(s) from " << dataPath << "\n";
    }

    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }

    SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == INVALID_SOCKET) {
        std::cerr << "cannot create socket\n";
        WSACleanup();
        return 1;
    }

    BOOL reuse = TRUE;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<unsigned short>(port));
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 只监听 127.0.0.1

    if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
        std::cerr << "cannot bind port " << port << " (already in use?)\n";
        closesocket(server);
        WSACleanup();
        return 1;
    }
    if (listen(server, 16) == SOCKET_ERROR) {
        std::cerr << "cannot listen\n";
        closesocket(server);
        WSACleanup();
        return 1;
    }

    const std::string url = "http://127.0.0.1:" + std::to_string(port) + "/";
    std::cout << "Task server: " << url << "\n"
              << "Data file:   " << dataPath << "\n"
              << "Close this window to stop.\n";
    std::cout.flush();

#ifdef _WIN32
    // 用默认浏览器打开页面；失败也不影响服务本身。
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif

    std::vector<Task> trash; // 最近删除的任务，用于撤销

    while (true) {
        SOCKET client = accept(server, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;

        const std::string request = readRequestText(client);
        std::string method = "GET";
        std::string target = "/";
        if (!request.empty()) {
            std::istringstream line(request.substr(0, request.find("\r\n")));
            line >> method >> target;
        }

        std::string path = target;
        std::string rawQuery;
        const std::size_t question = target.find('?');
        if (question != std::string::npos) {
            path = target.substr(0, question);
            rawQuery = target.substr(question + 1);
        }
        const Query query = parseQuery(rawQuery);

        const bool isUpload = method == "POST" && path == "/api/attach";
        if (method != "GET" && !isUpload) {
            respond(client, 405, "Method Not Allowed", "application/json",
                    errorJson("only GET is supported"));
        } else if (path == "/" || path == "/index.html") {
            respond(client, 200, "OK", "text/html", kIndexHtml);
        } else if (path == "/api/tasks") {
            const std::string sortKey = toLower(queryValue(query, "sort", "id"));
            SortMode mode = SortMode::Id;
            if (sortKey == "priority") mode = SortMode::Priority;
            else if (sortKey == "date") mode = SortMode::DueDate;

            std::vector<Task> tasks = manager.sorted(mode);
            const std::string keyword = toLower(queryValue(query, "q"));
            if (!keyword.empty()) {
                tasks.erase(std::remove_if(tasks.begin(), tasks.end(), [&](const Task& task) {
                    return toLower(task.title).find(keyword) == std::string::npos;
                }), tasks.end());
            }
            respond(client, 200, "OK", "application/json", tasksToJson(tasks));
        } else if (path == "/api/add") {
            const std::string title = queryValue(query, "title");
            const std::string due = queryValue(query, "due");
            std::string target = queryValue(query, "target");
            if (target == "-") target.clear();
            const std::string repeat = queryValue(query, "repeat");
            Priority priority = Priority::Medium;
            priorityFromValue(queryValue(query, "priority", "2"), priority);

            if (title.empty() || title.find_first_not_of(" \t") == std::string::npos) {
                respond(client, 400, "Bad Request", "application/json", errorJson("title is empty"));
            } else if (!target.empty() && !isValidDate(target)) {
                respond(client, 400, "Bad Request", "application/json",
                        errorJson("invalid target date, expected YYYY-MM-DD"));
            } else if (!datesConsistent(due, target)) {
                respond(client, 400, "Bad Request", "application/json",
                        errorJson("target completion date must not be later than the due date"));
            } else if (!manager.add(title, priority, due, target, repeat)) {
                respond(client, 400, "Bad Request", "application/json",
                        errorJson("invalid date or repeat rule"));
            } else {
                const std::vector<Task> all = manager.sorted(SortMode::Id);
                const int newId = all.empty() ? 0 : all.back().id;

                // 可选：创建时带标签，空格或逗号分隔
                std::string token;
                const std::string tagsText = queryValue(query, "tags");
                for (char c : tagsText) {
                    if (c == ' ' || c == ',' || c == '\t') {
                        if (!token.empty()) manager.addTag(newId, token);
                        token.clear();
                    } else {
                        token.push_back(c);
                    }
                }
                if (!token.empty()) manager.addTag(newId, token);

                manager.save(dataPath);
                respond(client, 200, "OK", "application/json",
                        "{\"ok\":true,\"id\":" + std::to_string(newId) + "}");
            }
        } else if (path == "/api/stats") {
            std::vector<std::pair<std::string, int>> days;
            for (int i = 83; i >= 0; --i) days.emplace_back(localDateString(-i), 0);

            std::size_t totalDone = 0;
            std::size_t total = 0;
            for (const Task& task : manager.sorted(SortMode::Id)) {
                ++total;
                if (task.doneAt.empty()) continue;
                ++totalDone;
                for (auto& day : days) {
                    if (day.first == task.doneAt) { ++day.second; break; }
                }
            }

            std::ostringstream out;
            out << "{\"today\":\"" << localDateString() << "\",\"done\":" << totalDone
                << ",\"total\":" << total << ",\"days\":[";
            for (std::size_t i = 0; i < days.size(); ++i) {
                if (i) out << ',';
                out << "{\"date\":\"" << days[i].first << "\",\"count\":" << days[i].second << "}";
            }
            out << "]}";
            respond(client, 200, "OK", "application/json", out.str());
        } else if (path == "/api/reviews") {
            std::vector<Review> items = loadReviews(reviewsPath(dataPath));
            std::sort(items.begin(), items.end(),
                      [](const Review& a, const Review& b) { return a.date < b.date; });
            std::reverse(items.begin(), items.end());

            std::ostringstream out;
            out << "{\"count\":" << items.size() << ",\"items\":[";
            for (std::size_t i = 0; i < items.size(); ++i) {
                if (i) out << ',';
                out << "{\"date\":\"" << jsonEscape(items[i].date) << "\""
                    << ",\"mood\":\"" << jsonEscape(items[i].mood) << "\""
                    << ",\"text\":\"" << jsonEscape(items[i].text) << "\"}";
            }
            out << "]}";
            respond(client, 200, "OK", "application/json", out.str());
        } else if (path == "/api/review") {
            const std::string date = queryValue(query, "date", localDateString());
            const std::string mood = queryValue(query, "mood");
            const std::string text = queryValue(query, "text");

            std::vector<Review> items = loadReviews(reviewsPath(dataPath));
            bool found = false;
            for (Review& item : items) {
                if (item.date == date) {
                    item.mood = mood;
                    item.text = text;
                    found = true;
                }
            }
            if (!found && (!text.empty() || !mood.empty())) {
                items.push_back(Review{date, mood, text});
            }
            items.erase(std::remove_if(items.begin(), items.end(),
                                       [](const Review& item) {
                                           return item.text.empty() && item.mood.empty();
                                       }),
                        items.end());
            std::sort(items.begin(), items.end(),
                      [](const Review& a, const Review& b) { return a.date < b.date; });

            if (!saveReviews(reviewsPath(dataPath), items)) {
                respond(client, 500, "Internal Server Error", "application/json",
                        errorJson("cannot write reviews"));
            } else {
                respond(client, 200, "OK", "application/json", "{\"ok\":true}");
            }
        } else if (path == "/api/trash") {
            std::ostringstream out;
            out << "{\"count\":" << trash.size() << ",\"items\":[";
            for (std::size_t i = 0; i < trash.size(); ++i) {
                const Task& item = trash[trash.size() - 1 - i]; // 最近删除的排前面
                if (i) out << ',';
                out << "{\"title\":\"" << jsonEscape(item.title) << "\""
                    << ",\"priority\":" << static_cast<int>(item.priority)
                    << ",\"due\":\"" << jsonEscape(item.due) << "\"}";
            }
            out << "]}";
            respond(client, 200, "OK", "application/json", out.str());
        } else if (path == "/api/restore") {
            int index = 0;
            if (!parseInt(queryValue(query, "index"), index) || index < 0 ||
                static_cast<std::size_t>(index) >= trash.size()) {
                respond(client, 400, "Bad Request", "application/json", errorJson("invalid index"));
            } else {
                const std::size_t at = trash.size() - 1 - static_cast<std::size_t>(index);
                const Task task = trash[at];
                trash.erase(trash.begin() + static_cast<std::ptrdiff_t>(at));
                manager.restore(task);
                manager.save(dataPath);
                respond(client, 200, "OK", "application/json", "{\"ok\":true}");
            }
        } else if (path == "/api/clear-done") {
            const std::size_t removed = manager.removeDone();
            manager.save(dataPath);
            respond(client, 200, "OK", "application/json",
                    "{\"ok\":true,\"removed\":" + std::to_string(removed) + "}");
        } else if (path == "/api/undo") {
            if (trash.empty()) {
                respond(client, 400, "Bad Request", "application/json", errorJson("nothing to undo"));
            } else {
                const Task task = trash.back();
                trash.pop_back();
                manager.restore(task);
                manager.save(dataPath);
                respond(client, 200, "OK", "application/json",
                        "{\"ok\":true,\"id\":" + std::to_string(task.id) + "}");
            }
        } else if (path == "/api/export.md") {
            std::ostringstream md;
            md << "# 任务手账\n\n";
            const std::vector<Task> all = manager.sorted(SortMode::Id);
            const char* groups[2][2] = {{"## 未完成\n", "## 已完成\n"}, {"- [ ] ", "- [x] "}};
            for (int done = 0; done < 2; ++done) {
                bool header = false;
                for (const Task& task : all) {
                    if ((done == 1) != task.done) continue;
                    if (!header) { md << groups[0][done]; header = true; }
                    md << groups[1][done] << task.title;
                    md << "　`" << priorityText(task.priority) << "`";
                    if (!task.target.empty()) md << "　🎯 " << task.target;
                    if (!task.due.empty()) md << "　⏳ " << task.due;
                    md << "\n";
                }
                if (header) md << "\n";
            }
            respond(client, 200, "OK", "text/markdown", md.str(),
                    "Content-Disposition: attachment; filename=tasks.md\r\n");
        } else if (path == "/api/detach") {
            int id = 0;
            if (!parseInt(queryValue(query, "id"), id)) {
                respond(client, 400, "Bad Request", "application/json", errorJson("invalid id"));
            } else if (!manager.contains(id)) {
                respond(client, 404, "Not Found", "application/json", errorJson("task not found"));
            } else if (!manager.removeFile(id, queryValue(query, "name"))) {
                respond(client, 404, "Not Found", "application/json", errorJson("no such attachment"));
            } else {
                manager.save(dataPath);
                respond(client, 200, "OK", "application/json", "{\"ok\":true}");
            }
        } else if (path == "/api/tagadd" || path == "/api/tagremove") {
            int id = 0;
            const std::string tag = queryValue(query, "tag");
            if (!parseInt(queryValue(query, "id"), id)) {
                respond(client, 400, "Bad Request", "application/json", errorJson("invalid id"));
            } else if (!manager.contains(id)) {
                respond(client, 404, "Not Found", "application/json", errorJson("task not found"));
            } else if (tag.empty()) {
                respond(client, 400, "Bad Request", "application/json", errorJson("empty tag"));
            } else {
                const bool ok = path == "/api/tagadd" ? manager.addTag(id, tag)
                                                      : manager.removeTag(id, tag);
                manager.save(dataPath);
                respond(client, ok ? 200 : 400, ok ? "OK" : "Bad Request", "application/json",
                        ok ? "{\"ok\":true}" : errorJson("tag unchanged"));
            }
        } else if (path == "/api/spent") {
            int id = 0;
            int seconds = 0;
            if (!parseInt(queryValue(query, "id"), id) ||
                !parseInt(queryValue(query, "seconds"), seconds) || seconds <= 0) {
                respond(client, 400, "Bad Request", "application/json", errorJson("invalid id or seconds"));
            } else if (!manager.contains(id)) {
                respond(client, 404, "Not Found", "application/json", errorJson("task not found"));
            } else {
                manager.addSpent(id, seconds);
                manager.save(dataPath);
                respond(client, 200, "OK", "application/json", "{\"ok\":true}");
            }
        } else if (path == "/api/sticker") {
            int id = 0;
            if (!parseInt(queryValue(query, "id"), id)) {
                respond(client, 400, "Bad Request", "application/json", errorJson("invalid id"));
            } else if (!manager.contains(id)) {
                respond(client, 404, "Not Found", "application/json", errorJson("task not found"));
            } else {
                manager.setSticker(id, queryValue(query, "sticker"));
                manager.save(dataPath);
                respond(client, 200, "OK", "application/json", "{\"ok\":true}");
            }
        } else if (path == "/api/notes") {            int id = 0;
            if (!parseInt(queryValue(query, "id"), id)) {
                respond(client, 400, "Bad Request", "application/json", errorJson("invalid id"));
            } else if (!manager.contains(id)) {
                respond(client, 404, "Not Found", "application/json", errorJson("task not found"));
            } else {
                manager.setNotes(id, queryValue(query, "notes"));
                manager.save(dataPath);
                respond(client, 200, "OK", "application/json", "{\"ok\":true}");
            }
        } else if (path == "/api/attach") {
            int id = 0;
            if (!parseInt(queryValue(query, "id"), id)) {
                respond(client, 400, "Bad Request", "application/json", errorJson("invalid id"));
            } else if (!manager.contains(id)) {
                respond(client, 404, "Not Found", "application/json", errorJson("task not found"));
            } else {
                const std::string body = readBody(client, request, contentLengthOf(request));
                if (body.empty()) {
                    respond(client, 400, "Bad Request", "application/json", errorJson("empty file"));
                } else {
                    const std::string dir = attachmentsDir(dataPath);
                    CreateDirectoryA(dir.c_str(), nullptr);
                    const std::string stored = std::to_string(id) + "_" +
                                               sanitizeName(queryValue(query, "name", "file"));
                    if (!writeFileBytes(dir + "\\" + stored, body)) {
                        respond(client, 500, "Internal Server Error", "application/json",
                                errorJson("cannot write attachment"));
                    } else {
                        manager.addFile(id, stored);
                        manager.save(dataPath);
                        respond(client, 200, "OK", "application/json",
                                "{\"ok\":true,\"name\":\"" + jsonEscape(stored) + "\"}");
                    }
                }
            }
        } else if (path == "/attach") {
            const std::string name = sanitizeName(queryValue(query, "name"));
            std::string data;
            if (!readFileBytes(attachmentsDir(dataPath) + "\\" + name, data)) {
                respond(client, 404, "Not Found", "application/json", errorJson("no such file"));
            } else {
                respond(client, 200, "OK", mimeOf(name), data,
                        "Content-Disposition: inline; filename=\"" + name + "\"\r\n");
            }
        } else if (path == "/api/subadd" || path == "/api/subtoggle" || path == "/api/subremove") {            int id = 0;
            int index = 0;
            const bool needIndex = path != "/api/subadd";
            if (!parseInt(queryValue(query, "id"), id) ||
                (needIndex && (!parseInt(queryValue(query, "index"), index) || index < 0))) {
                respond(client, 400, "Bad Request", "application/json", errorJson("invalid id or index"));
            } else if (!manager.contains(id)) {
                respond(client, 404, "Not Found", "application/json", errorJson("task not found"));
            } else {
                bool ok = false;
                if (path == "/api/subadd") {
                    ok = manager.addSub(id, queryValue(query, "title"));
                } else if (path == "/api/subtoggle") {
                    ok = manager.toggleSub(id, static_cast<std::size_t>(index));
                } else {
                    ok = manager.removeSub(id, static_cast<std::size_t>(index));
                }
                if (ok) manager.save(dataPath);
                respond(client, ok ? 200 : 400, ok ? "OK" : "Bad Request", "application/json",
                        ok ? "{\"ok\":true}" : errorJson("invalid subtask"));
            }
        } else if (path == "/api/done" || path == "/api/remove" || path == "/api/update") {
            int id = 0;
            if (!parseInt(queryValue(query, "id"), id)) {
                respond(client, 400, "Bad Request", "application/json", errorJson("invalid id"));
            } else if (!manager.contains(id)) {
                respond(client, 404, "Not Found", "application/json", errorJson("task not found"));
            } else {
                bool ok = true;
                std::string errorText = "invalid priority";
                if (path == "/api/done") {
                    // 同一个接口做开关：已完成就取消，未完成就标记完成
                    bool wasDone = false;
                    for (const Task& task : manager.sorted(SortMode::Id)) {
                        if (task.id == id) wasDone = task.done;
                    }
                    ok = wasDone ? manager.markUndone(id) : manager.markDone(id);
                    if (ok) manager.setDoneAt(id, wasDone ? "" : localDateString());

                    // 重复任务：刚被勾选完成时，自动生成下一次
                    if (ok && !wasDone) {
                        const Task* live = manager.find(id);
                        if (live != nullptr && !live->repeat.empty()) {
                            const Task source = *live; // 复制一份，add 会让迭代器失效
                            const std::string nextDue = shiftDate(source.due, source.repeat);
                            const std::string nextTarget = source.target.empty()
                                                               ? std::string()
                                                               : shiftDate(source.target, source.repeat);
                            if (manager.add(source.title, source.priority, nextDue, nextTarget,
                                            source.repeat)) {
                                const std::vector<Task> all = manager.sorted(SortMode::Id);
                                if (!all.empty()) {
                                    const int newId = all.back().id;
                                    manager.setNotes(newId, source.notes);
                                    for (const SubTask& sub : source.subs) {
                                        manager.addSub(newId, sub.title);
                                    }
                                }
                            }
                        }
                    }
                } else if (path == "/api/remove") {
                    const Task* victim = manager.find(id);
                    if (victim) {
                        trash.push_back(*victim); // 留着给撤销用
                        if (trash.size() > 30) trash.erase(trash.begin());
                    }
                    ok = manager.remove(id);
                } else {
                    const std::string priorityText = queryValue(query, "priority");
                    if (!priorityText.empty()) {
                        Priority priority = Priority::Medium;
                        if (!priorityFromValue(priorityText, priority)) ok = false;
                        else manager.setPriority(id, priority);
                    }

                    // 先算出生效后的日期再校验，避免出现目标晚于截止的半成品
                    std::string effectiveDue;
                    std::string effectiveTarget;
                    for (const Task& task : manager.sorted(SortMode::Id)) {
                        if (task.id == id) {
                            effectiveDue = task.due;
                            effectiveTarget = task.target;
                        }
                    }
                    const std::string due = queryValue(query, "due");
                    if (!due.empty()) effectiveDue = (due == "-") ? "" : due;
                    const std::string target = queryValue(query, "target");
                    if (!target.empty()) effectiveTarget = (target == "-") ? "" : target;

                    if (ok && !datesConsistent(effectiveDue, effectiveTarget)) {
                        ok = false;
                        errorText = "target completion date must not be later than the due date";
                    }
                    if (ok && !due.empty()) manager.setDue(id, effectiveDue);
                    if (ok && !target.empty()) manager.setTarget(id, effectiveTarget);
                }
                manager.save(dataPath);
                respond(client, ok ? 200 : 400, ok ? "OK" : "Bad Request", "application/json",
                        ok ? "{\"ok\":true}" : errorJson(errorText));
            }
        } else {
            respond(client, 404, "Not Found", "application/json", errorJson("unknown path"));
        }

        shutdown(client, SD_SEND);
        closesocket(client);
    }

    closesocket(server);
    WSACleanup();
    return 0;
}
