function next_cell( pos,dir )
	local dir2=dir/math.max(math.abs(dir[1]),math.abs(dir[2]))
	return pos+dir2
end
deck={}
deck.strike={
	--main
	name="Strike",
	cost=2,
	description="Perform an attack with a very big sword",
	--misc
	attack=3,
	range=2,
	--callbacks
	use=function ( card,game )
		local target=game.target_enemy(game.player:pos(),card.range)
		game.damage(target,card.attack)
		card:discard()
	end
}
deck.push={

	name="Push",
	cost=1,
	description="Force a move. Deal damage if can't move",

	attack=1,
	range=2,
	distance=3,

	use=function ( card,game )
		--first ask which one of enemies you want to push
		local target=game.target_enemy(game.player:pos(),card.range)
		--by default if the first "target" function is canceled. the whole
		--		card is not played and returned to the hand

		--figure out the direction of push
		local dir=target:pos()-game.player:pos()
		dir=dir:normalized()
		local start=target:pos()

		--raycast (i.e. like going straight in that direction)
		local target_cell=start+dir*card.distance
		local path=game.raycast(next_cell(start,dir),target_cell)
		--move the unit
		game.move(target,path)
		
		--then if the unit could not move ALL the way, hurt it
		if #path<card.distance-1 then
			game.damage(target,card.attack)
		end
		card:discard()
	end
}
deck.move={
	name="Run",
	cost=1,
	description="Run to the target",

	generated=true,
	range=5,
	move=true,

	use=function (card,game)
		local path=game.target_path(game.player:pos(),card.range)
		game.move(game.player,path)
		card:destroy()
	end
}
--alternative way of writing the same
local wound={
	name="Wound",
	description="Loose game if 3 are in hand at the end of turn",
	wound=true,
}
function wound:use(game)
	--get cards from "hand"
	local cards=game.get_cards(2)
	--filter cards: you can only choose to destroy non-wounds
	local tmp_cards={}
	for i,v in ipairs(cards) do
		if not v.wound then
			table.insert(tmp_cards,v)
		end
	end
	--ask which one
	local card_choice=game.target_card(tmp_cards)
	--if picked a card (e.g. you can be left with only wound in hand)
	if #card_choice then
		--destroy it
		card_choice[1]:destroy()
		--destroy self
		self:destroy() --NOTE when using this way of writing it's called "self" not card
	end
end

deck.wound=wound

return deck